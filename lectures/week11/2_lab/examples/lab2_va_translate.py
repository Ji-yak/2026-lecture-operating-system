#!/usr/bin/env python3
"""
va_translate.py - RISC-V Sv39 가상 주소 변환 시각화 도구

[개요]
이 스크립트는 RISC-V Sv39 방식의 가상 주소를 입력받아
3단계 페이지 테이블 워크(page table walk) 과정을 시각적으로 보여줍니다.

[핵심 개념: Sv39 페이지 테이블 구조]
RISC-V의 Sv39 방식은 39비트 가상 주소를 사용하며, 3단계 페이지 테이블로 변환합니다.

  가상 주소 (39비트):
    +----------+----------+----------+--------------+
    | VPN[2]   | VPN[1]   | VPN[0]   | Page Offset  |
    | (9 bits) | (9 bits) | (9 bits) | (12 bits)    |
    +----------+----------+----------+--------------+
    비트 38~30   비트 29~21  비트 20~12   비트 11~0

  3단계 페이지 테이블 워크:
    1) satp 레지스터에서 L2(루트) 페이지 테이블의 물리 주소를 읽음
    2) L2 테이블에서 VPN[2]를 인덱스로 PTE를 찾고, PPN을 추출하여 L1 테이블 위치를 결정
    3) L1 테이블에서 VPN[1]을 인덱스로 PTE를 찾고, PPN을 추출하여 L0 테이블 위치를 결정
    4) L0 테이블에서 VPN[0]을 인덱스로 PTE를 찾고, PPN을 추출하여 물리 페이지 시작 주소를 결정
    5) 물리 페이지 시작 주소 + Page Offset = 최종 물리 주소

  PTE (Page Table Entry) 구조 (64비트):
    비트 63~54: 예약
    비트 53~10: PPN (Physical Page Number) - 다음 레벨 테이블 또는 물리 페이지의 주소
    비트  9~ 8: RSW (소프트웨어 예약)
    비트  7: D (Dirty) - 페이지에 쓰기가 발생했는지 여부
    비트  6: A (Accessed) - 페이지에 접근이 발생했는지 여부
    비트  5: G (Global) - 전역 매핑 여부
    비트  4: U (User) - 사용자 모드에서 접근 가능 여부
    비트  3: X (eXecute) - 실행 가능 여부
    비트  2: W (Write) - 쓰기 가능 여부
    비트  1: R (Read) - 읽기 가능 여부
    비트  0: V (Valid) - PTE가 유효한지 여부

  xv6의 PX 매크로 (kernel/riscv.h):
    #define PX(level, va)  ((((uint64)(va)) >> (PGSHIFT + 9*(level))) & 0x1FF)
    - PX(2, va): VPN[2] 추출 (L2 인덱스)
    - PX(1, va): VPN[1] 추출 (L1 인덱스)
    - PX(0, va): VPN[0] 추출 (L0 인덱스)

사용법:
    python3 va_translate.py                 # 대화형 모드
    python3 va_translate.py 0x80001234      # 인자로 가상 주소 전달
    python3 va_translate.py 0x1000          # 유저 영역 주소 예시
"""

import sys


# ============================================================
# Sv39 상수 정의
# ============================================================
# Sv39에서 페이지 크기는 4KB (2^12 = 4096 bytes)
# 각 페이지 테이블은 512개의 PTE를 포함 (2^9 = 512, 각 PTE는 8바이트)
# 따라서 하나의 페이지 테이블 자체도 정확히 한 페이지 크기 (512 * 8 = 4096)
PGSIZE = 4096           # 4KB 페이지 크기
PGSHIFT = 12            # 페이지 오프셋 비트 수 (log2(4096) = 12)
LEVELS = 3              # 3단계 페이지 테이블 (L2 -> L1 -> L0)
PTE_BITS = 9            # 각 레벨의 인덱스 비트 수 (log2(512) = 9)
MAXVA = 1 << 38         # xv6에서 사용하는 최대 VA (비트 38은 사용하지 않음)
                        # 참고: Sv39는 비트 38~0 총 39비트를 사용하지만,
                        # xv6는 MAXVA를 1<<38로 정의하여 비트 38을 미사용

# xv6 메모리 레이아웃 상수 (kernel/memlayout.h 참조)
# xv6 커널은 가상 주소 = 물리 주소인 직접 매핑(direct mapping)을 사용
KERNBASE   = 0x80000000                     # 커널 코드/데이터 시작 주소 (DRAM 시작)
PHYSTOP    = KERNBASE + 128 * 1024 * 1024   # 물리 메모리 끝 (0x88000000, 128MB)
UART0      = 0x10000000                     # 시리얼 포트 I/O 장치 주소
VIRTIO0    = 0x10001000                     # VirtIO 디스크 장치 주소
PLIC       = 0x0C000000                     # 플랫폼 인터럽트 컨트롤러 주소
TRAMPOLINE = MAXVA - PGSIZE                 # 트램폴린 페이지 (VA 최상위, 커널/유저 공유)
TRAPFRAME  = TRAMPOLINE - PGSIZE            # 트랩프레임 (트랩 시 레지스터 저장용)


def extract_fields(va):
    """
    가상 주소에서 Sv39 필드를 추출합니다.

    Sv39 가상 주소 구조 (39비트):
      비트 38~30: VPN[2] (L2 인덱스)
      비트 29~21: VPN[1] (L1 인덱스)
      비트 20~12: VPN[0] (L0 인덱스)
      비트 11~ 0: Page Offset
    """
    offset = va & 0xFFF                    # 비트 11~0
    vpn0   = (va >> 12) & 0x1FF            # 비트 20~12
    vpn1   = (va >> 21) & 0x1FF            # 비트 29~21
    vpn2   = (va >> 30) & 0x1FF            # 비트 38~30

    return vpn2, vpn1, vpn0, offset


def format_binary(value, bits):
    """값을 지정된 비트 수의 2진수 문자열로 변환합니다."""
    return format(value, f'0{bits}b')


def identify_region(va):
    """
    xv6에서 이 가상 주소가 어떤 영역에 해당하는지 판별합니다.

    xv6 커널 주소 공간은 대부분 직접 매핑(VA == PA)을 사용합니다.
    예외: TRAMPOLINE과 TRAPFRAME은 가상 주소 최상위에 매핑되어 있으며,
          실제 물리 주소와 다릅니다 (TRAMPOLINE은 커널 코드 내 trampoline.S,
          TRAPFRAME은 프로세스별로 할당된 물리 페이지).
    """
    if va >= TRAMPOLINE and va < TRAMPOLINE + PGSIZE:
        return "TRAMPOLINE (트랩 핸들러 코드)"
    elif va >= TRAPFRAME and va < TRAPFRAME + PGSIZE:
        return "TRAPFRAME (트랩 프레임)"
    elif va >= KERNBASE and va < PHYSTOP:
        if va < KERNBASE + 0x100000:  # 대략적인 etext 위치
            return "커널 텍스트 (Kernel Text) - 직접 매핑"
        else:
            return "커널 데이터/RAM (Kernel Data) - 직접 매핑"
    elif va >= PLIC and va < PLIC + 0x4000000:
        return "PLIC (인터럽트 컨트롤러) - 직접 매핑"
    elif va >= UART0 and va < UART0 + PGSIZE:
        return "UART0 (시리얼 포트) - 직접 매핑"
    elif va >= VIRTIO0 and va < VIRTIO0 + PGSIZE:
        return "VIRTIO0 (디스크 I/O) - 직접 매핑"
    elif va < 0x10000:
        return "유저 프로세스 영역 (User Space) - 코드/데이터/스택"
    elif va < KERNBASE:
        return "유저 프로세스 영역 (User Space)"
    else:
        return "알 수 없는 영역"


def print_translation(va):
    """가상 주소의 Sv39 변환 과정을 상세히 출력합니다."""

    vpn2, vpn1, vpn0, offset = extract_fields(va)

    print()
    print("=" * 72)
    print(f"  RISC-V Sv39 가상 주소 변환")
    print("=" * 72)
    print()

    # 입력 가상 주소 출력
    print(f"  입력 가상 주소: 0x{va:016x} ({va})")
    print(f"  영역:           {identify_region(va)}")
    print()

    # VA 유효성 검사
    if va >= (1 << 39):
        print(f"  [경고] 이 주소는 39비트를 초과합니다!")
        print(f"         Sv39에서 비트 63~39는 반드시 0이어야 합니다.")
        print(f"         xv6의 MAXVA = 0x{MAXVA:x} (비트 38은 사용하지 않음)")
        print()

    # 39비트 2진수 표현
    va39 = va & ((1 << 39) - 1)  # 하위 39비트만 추출
    binary_str = format_binary(va39, 39)

    # 필드별 분리 표시
    vpn2_bin = binary_str[0:9]
    vpn1_bin = binary_str[9:18]
    vpn0_bin = binary_str[18:27]
    offset_bin = binary_str[27:39]

    print("  39비트 가상 주소 구조:")
    print()
    print(f"    비트:  38       30 29       21 20       12 11            0")
    print(f"          +---------+-----------+-----------+--------------+")
    print(f"          |{vpn2_bin:^9s}|{vpn1_bin:^11s}|{vpn0_bin:^11s}|{offset_bin:^14s}|")
    print(f"          +---------+-----------+-----------+--------------+")
    print(f"           VPN[2]     VPN[1]      VPN[0]     Page Offset")
    print()

    # 각 필드 상세 정보
    print("  필드별 분석:")
    print(f"  ---------------------------------------------------------------")
    print(f"  VPN[2] (L2 인덱스) = {vpn2:>3d} (0x{vpn2:03x}, 0b{format_binary(vpn2, 9)})")
    print(f"  VPN[1] (L1 인덱스) = {vpn1:>3d} (0x{vpn1:03x}, 0b{format_binary(vpn1, 9)})")
    print(f"  VPN[0] (L0 인덱스) = {vpn0:>3d} (0x{vpn0:03x}, 0b{format_binary(vpn0, 9)})")
    print(f"  Offset (오프셋)    = {offset:>4d} (0x{offset:03x}, 0b{format_binary(offset, 12)})")
    print()

    # 변환 과정 시각화
    print("  3단계 페이지 테이블 변환 과정:")
    print(f"  ---------------------------------------------------------------")
    print()
    print(f"  [1단계] satp 레지스터 -> L2 테이블 시작 주소")
    print(f"          L2 테이블[{vpn2}] 의 PTE를 읽음")
    print(f"          PTE에서 PPN 추출 -> L1 테이블의 물리 주소")
    print()
    print(f"  [2단계] L1 테이블[{vpn1}] 의 PTE를 읽음")
    print(f"          PTE에서 PPN 추출 -> L0 테이블의 물리 주소")
    print()
    print(f"  [3단계] L0 테이블[{vpn0}] 의 PTE를 읽음")
    print(f"          PTE에서 PPN 추출 -> 물리 페이지의 시작 주소")
    print()
    print(f"  [최종]  물리 페이지 시작 주소 + offset(0x{offset:03x})")
    print(f"          = 최종 물리 주소")
    print()

    # xv6 매크로 동작 시뮬레이션
    print("  xv6 매크로 계산 결과:")
    print(f"  ---------------------------------------------------------------")
    print(f"  PX(2, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*2}) & 0x1FF = {vpn2}")
    print(f"  PX(1, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*1}) & 0x1FF = {vpn1}")
    print(f"  PX(0, 0x{va:x}) = (0x{va:x} >> {PGSHIFT + 9*0}) & 0x1FF = {vpn0}")
    print()

    # 직접 매핑인 경우 물리 주소도 표시
    if (va >= KERNBASE and va < PHYSTOP) or \
       (va >= UART0 and va < UART0 + PGSIZE) or \
       (va >= VIRTIO0 and va < VIRTIO0 + PGSIZE) or \
       (va >= PLIC and va < PLIC + 0x4000000):
        print(f"  [참고] 이 주소는 커널 직접 매핑 영역입니다.")
        print(f"         물리 주소(PA) = 가상 주소(VA) = 0x{va:016x}")
        print()

    # VA가 속하는 가상 페이지 정보
    page_va = va & ~0xFFF  # PGROUNDDOWN
    print(f"  가상 페이지 정보:")
    print(f"  ---------------------------------------------------------------")
    print(f"  이 VA가 속하는 가상 페이지 시작 주소: 0x{page_va:016x}")
    print(f"  페이지 내 오프셋: {offset} 바이트 (0x{offset:03x})")
    print(f"  가상 페이지 번호: {va >> 12} (0x{va >> 12:x})")
    print()


def print_example_table():
    """
    자주 사용되는 xv6 주소 예제를 출력합니다.

    각 주소에 대해 VPN[2], VPN[1], VPN[0], Offset을 계산하여
    3단계 페이지 테이블 워크에서 어떤 인덱스가 사용되는지 보여줍니다.
    이를 통해 xv6의 메모리 레이아웃과 페이지 테이블 구조의 관계를 이해할 수 있습니다.
    """
    print()
    print("=" * 72)
    print("  xv6 주요 가상 주소 예제")
    print("=" * 72)
    print()
    print(f"  {'주소':<22s} {'L2':>5s} {'L1':>5s} {'L0':>5s} {'오프셋':>6s}   영역")
    print(f"  {'-'*72}")

    examples = [
        (0x0000,      "유저 코드 시작"),
        (0x1000,      "유저 2번째 페이지"),
        (UART0,       "UART0"),
        (VIRTIO0,     "VIRTIO0"),
        (PLIC,        "PLIC"),
        (KERNBASE,    "KERNBASE"),
        (KERNBASE + 0x1234, "커널 텍스트 내부"),
        (PHYSTOP - 1, "PHYSTOP 직전"),
        (TRAPFRAME,   "TRAPFRAME"),
        (TRAMPOLINE,  "TRAMPOLINE"),
    ]

    for va, name in examples:
        vpn2, vpn1, vpn0, offset = extract_fields(va)
        print(f"  0x{va:016x}  {vpn2:>5d} {vpn1:>5d} {vpn0:>5d} 0x{offset:>03x}   {name}")

    print()


def interactive_mode():
    """
    대화형 모드로 가상 주소를 입력받습니다.

    사용자가 16진수(0x...) 또는 10진수 형태의 가상 주소를 입력하면,
    Sv39 방식으로 분해하여 3단계 페이지 테이블 워크 과정을 출력합니다.
    'q', 'quit', 'exit' 또는 Ctrl+C로 종료할 수 있습니다.
    """
    print()
    print("=" * 72)
    print("  RISC-V Sv39 가상 주소 변환 도구")
    print("  (종료: q 또는 Ctrl+C)")
    print("=" * 72)

    # 먼저 예제 테이블 출력
    print_example_table()

    while True:
        try:
            user_input = input("  가상 주소를 입력하세요 (예: 0x80001234): ").strip()

            if user_input.lower() in ('q', 'quit', 'exit'):
                print("  종료합니다.")
                break

            if user_input == '':
                continue

            # 입력 파싱 (0x 접두사 지원)
            if user_input.startswith('0x') or user_input.startswith('0X'):
                va = int(user_input, 16)
            else:
                va = int(user_input)

            print_translation(va)

        except ValueError:
            print(f"  [오류] 유효하지 않은 주소입니다: {user_input}")
            print(f"         16진수(0x...)나 10진수를 입력하세요.")
            print()
        except (KeyboardInterrupt, EOFError):
            print("\n  종료합니다.")
            break


def main():
    """
    메인 함수: 명령줄 인자가 있으면 해당 주소를 변환하고,
    없으면 대화형 모드로 진입합니다.
    """
    if len(sys.argv) > 1:
        # 명령줄 인자 모드: 각 인자를 가상 주소로 파싱하여 변환
        for arg in sys.argv[1:]:
            try:
                if arg.startswith('0x') or arg.startswith('0X'):
                    va = int(arg, 16)
                else:
                    va = int(arg)
                print_translation(va)
            except ValueError:
                print(f"  [오류] 유효하지 않은 주소: {arg}")
                sys.exit(1)

        # 추가로 예제 테이블도 출력
        print_example_table()
    else:
        # 대화형 모드
        interactive_mode()


if __name__ == "__main__":
    main()
