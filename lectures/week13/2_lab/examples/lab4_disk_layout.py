#!/usr/bin/env python3
"""
disk_layout.py - xv6 디스크 레이아웃 시각화 스크립트

[개요]
xv6 파일 시스템의 디스크 블록 배치를 시각적으로 보여줍니다.
kernel/param.h, kernel/fs.h, mkfs/mkfs.c의 상수를 기반으로 계산합니다.

[xv6 디스크 레이아웃 구조]
xv6 파일 시스템은 디스크를 다음과 같은 영역으로 나눕니다:

  +-------+-------+-------+-------+--------+------------------+
  | Boot  | Super | Log   | Inode | Bitmap | Data blocks      |
  | block | block | area  | area  | area   |                  |
  +-------+-------+-------+-------+--------+------------------+
  블록 0   블록 1   블록 2~  블록 33~ 블록 46   블록 47~1999

  1) Boot Block (블록 0):
     - 부트 로더가 저장되는 영역 (xv6에서는 실제로 사용하지 않음)

  2) Super Block (블록 1):
     - 파일 시스템의 메타데이터: 전체 크기, 각 영역의 크기와 시작 위치
     - struct superblock: { magic, size, nblocks, ninodes, nlog, logstart,
                            inodestart, bmapstart }

  3) Log Area (블록 2~32):
     - WAL(Write-Ahead Logging)을 위한 영역
     - 로그 헤더(1블록) + 로그 데이터 블록(LOGBLOCKS=30개)
     - crash consistency 보장: 모든 디스크 수정은 먼저 로그에 기록
     - commit 후 실제 위치로 복사 (install_trans)

  4) Inode Area (블록 33~45):
     - 디스크 inode(struct dinode) 배열이 저장되는 영역
     - 각 inode는 64바이트, 한 블록(1024)에 16개(IPB)의 inode가 들어감
     - struct dinode: { type, major, minor, nlink, size, addrs[13] }

  5) Bitmap Area (블록 46):
     - 데이터 블록의 사용/미사용 상태를 비트맵으로 관리
     - 각 비트가 하나의 데이터 블록에 대응 (1=사용중, 0=미사용)
     - balloc()에서 빈 블록을 찾을 때 사용

  6) Data Blocks (블록 47~1999):
     - 실제 파일 데이터와 디렉토리 엔트리가 저장되는 영역
     - inode의 addrs[] 배열이 이 영역의 블록을 가리킴

실행:
    python3 disk_layout.py

교육 목적: xv6 파일 시스템 수업에서 디스크 구조를 이해하기 위한 도구
"""

# ============================================================
# xv6 파일 시스템 상수 (kernel/param.h, kernel/fs.h 기준)
# ============================================================

# 파일 시스템 전체 크기 및 블록 관련 상수
FSSIZE = 2000           # 전체 파일 시스템 크기 (블록 수, mkfs/mkfs.c에서 정의)
BSIZE = 1024            # 블록 크기 (bytes, kernel/fs.h의 BSIZE)
                        # xv6는 1KB 블록을 사용 (Linux 등은 보통 4KB)

# 로그(WAL) 관련 상수 - crash consistency를 보장하기 위한 설정
MAXOPBLOCKS = 10        # 하나의 FS 연산이 쓸 수 있는 최대 블록 수 (kernel/param.h)
                        # 예: 파일 쓰기 시 inode 블록 + 데이터 블록 + 비트맵 등
LOGBLOCKS = MAXOPBLOCKS * 3   # 로그 데이터 블록 수 (30) - 동시 트랜잭션 허용
NBUF = MAXOPBLOCKS * 3  # Buffer cache 크기 (30개 블록을 메모리에 캐시)

# inode 관련 상수
NINODES = 200           # 최대 inode 수 (mkfs/mkfs.c에서 정의)
                        # 파일 시스템에 생성 가능한 최대 파일+디렉토리 수
NDIRECT = 12            # 직접 블록 포인터 수 (struct dinode의 addrs[0]~addrs[11])
BSIZE_UINT = BSIZE // 4 # 한 블록에 들어가는 uint 수 (1024/4 = 256 = NINDIRECT)
NINDIRECT = BSIZE_UINT  # 간접 블록 포인터 수 (간접 블록 하나에 256개의 블록 주소)

# 구조체 크기 관련 상수
DINODE_SIZE = 64        # sizeof(struct dinode) = short*4 + uint + uint*13 = 64 bytes
                        # type(2) + major(2) + minor(2) + nlink(2) + size(4) + addrs(52)
IPB = BSIZE // DINODE_SIZE   # 블록당 inode 수 (1024/64 = 16개)
BPB = BSIZE * 8         # 블록당 비트맵 비트 수 (1024*8 = 8192비트)
                        # 하나의 비트맵 블록으로 8192개의 데이터 블록 관리 가능

DIRENT_SIZE = 16        # sizeof(struct dirent) = ushort(2) + char[14] = 16 bytes
                        # 디렉토리 엔트리: inode 번호(2바이트) + 파일 이름(최대 14자)


def calculate_layout():
    """
    xv6 디스크 레이아웃 계산 (mkfs/mkfs.c의 로직과 동일)

    mkfs는 빈 디스크 이미지(fs.img)를 생성할 때 이 계산을 수행하여
    각 영역의 시작 위치와 크기를 결정합니다.
    계산 결과는 superblock에 기록되어 커널이 부팅 시 읽어 사용합니다.
    """

    nlog = LOGBLOCKS + 1          # 로그 헤더(1블록) + 데이터 블록(30개) = 31블록
    ninodeblocks = NINODES // IPB + 1   # 200개 inode / 블록당 16개 + 1 = 13블록
    nbitmap = FSSIZE // BPB + 1         # 2000블록 / 8192비트 + 1 = 1블록

    # 메타 블록 합계: 부트(1) + 슈퍼(1) + 로그(31) + inode(13) + 비트맵(1) = 47
    nmeta = 1 + 1 + nlog + ninodeblocks + nbitmap
    nblocks = FSSIZE - nmeta            # 데이터 블록 수 = 2000 - 47 = 1953

    layout = {
        "boot":     {"start": 0,            "count": 1},
        "super":    {"start": 1,            "count": 1},
        "log":      {"start": 2,            "count": nlog},
        "inode":    {"start": 2 + nlog,     "count": ninodeblocks},
        "bitmap":   {"start": 2 + nlog + ninodeblocks, "count": nbitmap},
        "data":     {"start": nmeta,        "count": nblocks},
    }

    return layout, nmeta, nblocks


def print_header():
    """제목 출력"""
    print("=" * 70)
    print("  xv6 파일 시스템 디스크 레이아웃 시각화")
    print("=" * 70)
    print()


def print_constants():
    """주요 상수 출력"""
    print("[주요 상수]")
    print(f"  FSSIZE       = {FSSIZE:>6} 블록    (전체 파일 시스템 크기)")
    print(f"  BSIZE        = {BSIZE:>6} bytes   (블록 크기)")
    print(f"  LOGBLOCKS    = {LOGBLOCKS:>6}         (로그 데이터 블록 수)")
    print(f"  NBUF         = {NBUF:>6}         (Buffer cache 크기)")
    print(f"  NINODES      = {NINODES:>6}         (최대 inode 수)")
    print(f"  NDIRECT      = {NDIRECT:>6}         (직접 블록 포인터 수)")
    print(f"  NINDIRECT    = {NINDIRECT:>6}         (간접 블록 포인터 수)")
    print(f"  IPB          = {IPB:>6}         (블록당 inode 수)")
    print(f"  BPB          = {BPB:>6}         (블록당 비트맵 비트 수)")
    print(f"  MAXOPBLOCKS  = {MAXOPBLOCKS:>6}         (FS 연산당 최대 쓰기 블록)")
    print()


def print_layout_table(layout, nmeta, nblocks):
    """레이아웃 테이블 출력"""
    total_size_kb = FSSIZE * BSIZE / 1024

    print("[디스크 레이아웃 상세]")
    print(f"  전체 크기: {FSSIZE} 블록 = {FSSIZE * BSIZE:,} bytes"
          f" = {total_size_kb:.0f} KB")
    print(f"  메타 블록: {nmeta} 블록")
    print(f"  데이터 블록: {nblocks} 블록")
    print()

    print(f"  {'영역':<12} {'시작 블록':>10} {'블록 수':>10} "
          f"{'끝 블록':>10} {'크기 (bytes)':>14} {'설명'}")
    print("  " + "-" * 78)

    descriptions = {
        "boot":   "부트 로더 (xv6에서 미사용)",
        "super":  "superblock (FS 메타데이터)",
        "log":    f"WAL 로그 (헤더 1 + 데이터 {LOGBLOCKS})",
        "inode":  f"디스크 inode ({NINODES}개, {IPB}개/블록)",
        "bitmap": f"블록 사용 비트맵 ({BPB}비트/블록)",
        "data":   "실제 파일 데이터",
    }

    for name in ["boot", "super", "log", "inode", "bitmap", "data"]:
        info = layout[name]
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        size = count * BSIZE
        desc = descriptions[name]
        print(f"  {name:<12} {start:>10} {count:>10} "
              f"{end:>10} {size:>14,} {desc}")

    print()


def print_visual_layout(layout):
    """시각적 디스크 맵 출력"""
    print("[디스크 블록 맵]")
    print()

    # 컴팩트 뷰
    regions = [
        ("boot",   layout["boot"],   "B"),
        ("super",  layout["super"],  "S"),
        ("log",    layout["log"],    "L"),
        ("inode",  layout["inode"],  "I"),
        ("bitmap", layout["bitmap"], "M"),
        ("data",   layout["data"],   "D"),
    ]

    # 비례 시각화 (폭 60자)
    bar_width = 60
    total = FSSIZE

    print("  블록 0" + " " * (bar_width - 12) + f"블록 {FSSIZE - 1}")
    print("  |" + " " * (bar_width - 2) + "|")
    bar = ""
    labels = []
    for name, info, char in regions:
        count = info["count"]
        # 최소 1자 보장
        width = max(1, round(count / total * bar_width))
        bar += char * width
        labels.append((name, info["start"], char))

    # 정확히 bar_width에 맞추기
    if len(bar) > bar_width:
        bar = bar[:bar_width]
    elif len(bar) < bar_width:
        bar = bar + "D" * (bar_width - len(bar))

    print(f"  [{bar}]")
    print()

    # 범례
    print("  범례:")
    for name, info, char in regions:
        start = info["start"]
        count = info["count"]
        end = start + count - 1
        pct = count / total * 100
        print(f"    {char} = {name:<8} (블록 {start:>4} ~ {end:>4},"
              f" {count:>4}블록, {pct:5.1f}%)")

    print()

    # 상세 ASCII art
    print("  상세 다이어그램:")
    print()
    print("  +------+------+---------------------------+")
    log_info = layout["log"]
    print(f"  | boot | super|         log               |")
    print(f"  |  [0] |  [1] |  [{log_info['start']}]"
          f" ~ [{log_info['start'] + log_info['count'] - 1}]"
          f"              |")
    print("  +------+------+---------------------------+")

    inode_info = layout["inode"]
    bitmap_info = layout["bitmap"]
    print(f"  |       inode blocks         | bitmap     |")
    print(f"  | [{inode_info['start']}]"
          f" ~ [{inode_info['start'] + inode_info['count'] - 1}]"
          f"               |   [{bitmap_info['start']}]      |")
    print("  +----------------------------+-----------+")

    data_info = layout["data"]
    print(f"  |                data blocks                |")
    print(f"  |  [{data_info['start']}]"
          f" ~ [{data_info['start'] + data_info['count'] - 1}]"
          f"                           |")
    print("  +-------------------------------------------+")
    print()


def print_inode_structure():
    """
    inode 구조 시각화

    inode는 파일의 메타데이터를 저장하는 핵심 구조체입니다.
    디스크에는 struct dinode로, 메모리에는 struct inode로 존재합니다.

    중요한 차이:
    - struct dinode (kernel/fs.h): 디스크에 저장되는 64바이트 구조체
    - struct inode (kernel/file.h): 메모리에 캐시되는 확장 구조체
      디스크 inode + ref(참조카운트) + valid(디스크 로드 여부) + lock 등 추가

    블록 주소 매핑 (addrs[]):
    - addrs[0]~addrs[11]: 직접(direct) 블록 포인터 -> 12개 블록 직접 참조
    - addrs[12]: 간접(indirect) 블록 포인터 -> 256개 블록 간접 참조
    - 총 12 + 256 = 268 블록 = 268KB가 하나의 파일 최대 크기
    """
    print("[inode 블록 주소 구조]")
    print()
    print("  struct dinode {")
    print("    short type;          // 파일 유형 (2 bytes)")
    print("    short major;         // 장치 번호  (2 bytes)")
    print("    short minor;         //            (2 bytes)")
    print("    short nlink;         // 링크 수    (2 bytes)")
    print("    uint  size;          // 파일 크기  (4 bytes)")
    print(f"    uint  addrs[{NDIRECT}+1];  // 블록 주소  ({(NDIRECT+1)*4} bytes)")
    print(f"  }};  // 총 {DINODE_SIZE} bytes")
    print()
    print(f"  블록당 inode 수 = BSIZE / sizeof(dinode)"
          f" = {BSIZE} / {DINODE_SIZE} = {IPB}")
    print()

    print("  addrs[] 구조:")
    print()
    print("  addrs[0]  -----> [데이터 블록]")
    print("  addrs[1]  -----> [데이터 블록]")
    print("    ...              ...")
    print(f"  addrs[{NDIRECT - 1}] -----> [데이터 블록]")
    print(f"                      총 {NDIRECT}개 직접 블록"
          f" = {NDIRECT * BSIZE:,} bytes")
    print()
    print(f"  addrs[{NDIRECT}] -----> [간접 블록 (1024 bytes)]")
    print("                     |")
    print("                     +---> addr[0]   -> [데이터 블록]")
    print("                     +---> addr[1]   -> [데이터 블록]")
    print("                     +---> ...          ...")
    print(f"                     +---> addr[{NINDIRECT - 1}] -> [데이터 블록]")
    print(f"                      총 {NINDIRECT}개 간접 블록"
          f" = {NINDIRECT * BSIZE:,} bytes")
    print()


def print_file_size_limits():
    """
    파일 크기 제한 계산

    xv6 lab에서 bigfile 과제: single indirect만 있는 xv6에
    double indirect를 추가하여 최대 파일 크기를 확장하는 실습.
    이 함수는 각 indirection 레벨에 따른 최대 파일 크기를 계산합니다.
    """
    print("[파일 크기 제한 분석]")
    print()

    # 현재 제한
    maxfile = NDIRECT + NINDIRECT
    max_bytes = maxfile * BSIZE
    print("  현재 xv6 (direct + single indirect):")
    print(f"    직접 블록:   {NDIRECT:>10} 블록")
    print(f"    간접 블록:   {NINDIRECT:>10} 블록")
    print(f"    합계 (MAXFILE): {maxfile:>7} 블록")
    print(f"    최대 파일 크기: {max_bytes:>7,} bytes"
          f" = {max_bytes / 1024:.0f} KB")
    print()

    # Double indirect 추가 시
    double_indirect = NINDIRECT * NINDIRECT
    maxfile_di = NDIRECT + NINDIRECT + double_indirect
    max_bytes_di = maxfile_di * BSIZE
    print("  Double indirect 추가 시:")
    print(f"    직접 블록:            {NDIRECT:>10} 블록")
    print(f"    단일 간접 블록:       {NINDIRECT:>10} 블록")
    print(f"    이중 간접 블록: {NINDIRECT}x{NINDIRECT} = "
          f"{double_indirect:>6} 블록")
    print(f"    합계:              {maxfile_di:>10} 블록")
    print(f"    최대 파일 크기:    {max_bytes_di:>10,} bytes"
          f" = {max_bytes_di / 1024 / 1024:.2f} MB")
    print()

    # Triple indirect 추가 시
    triple_indirect = NINDIRECT * NINDIRECT * NINDIRECT
    maxfile_ti = NDIRECT + NINDIRECT + double_indirect + triple_indirect
    max_bytes_ti = maxfile_ti * BSIZE
    print("  Triple indirect 추가 시:")
    print(f"    직접 블록:               {NDIRECT:>12} 블록")
    print(f"    단일 간접 블록:          {NINDIRECT:>12} 블록")
    print(f"    이중 간접 블록:          {double_indirect:>12} 블록")
    print(f"    삼중 간접 블록: {NINDIRECT}^3 = {triple_indirect:>8} 블록")
    print(f"    합계:                 {maxfile_ti:>12} 블록")
    print(f"    최대 파일 크기:       {max_bytes_ti:>12,} bytes"
          f" = {max_bytes_ti / 1024 / 1024 / 1024:.2f} GB")
    print()


def print_log_structure():
    """
    로그(WAL: Write-Ahead Logging) 구조 시각화

    xv6의 로그는 crash consistency를 보장하는 핵심 메커니즘입니다.

    동작 원리:
    1. begin_op(): 트랜잭션 시작 (로그 공간이 부족하면 대기)
    2. log_write(): 수정할 블록을 로그에 기록 예약 (pin)
    3. end_op(): 트랜잭션 종료
       - 마지막 트랜잭션이면 commit() 호출:
         a) write_log(): 캐시에서 로그 영역으로 블록 복사
         b) write_head(): 로그 헤더의 n을 기록 (커밋 포인트!)
            이 시점 이후에 crash가 발생해도 복구 가능
         c) install_trans(): 로그에서 원래 위치로 블록 복사
         d) write_head(): n=0으로 초기화 (로그 삭제)

    crash 복구 (initlog에서 호출):
    - 부팅 시 로그 헤더를 읽어 n > 0이면 install_trans() 재실행
    - 이미 커밋된 트랜잭션만 복구하므로 원자성 보장
    """
    print("[로그 영역 구조]")
    print()
    print(f"  MAXOPBLOCKS = {MAXOPBLOCKS}")
    print(f"  LOGBLOCKS   = MAXOPBLOCKS * 3 = {LOGBLOCKS}")
    print(f"  로그 영역   = 헤더(1) + 데이터({LOGBLOCKS}) = {LOGBLOCKS + 1} 블록")
    print()
    print("  +----------+----------+----------+-----+----------+")
    print("  | log      | log      | log      | ... | log      |")
    print("  | header   | block 0  | block 1  |     | block 29 |")
    print("  | (블록 2) | (블록 3) | (블록 4) |     | (블록 32)|")
    print("  +----------+----------+----------+-----+----------+")
    print()
    print("  log header 구조:")
    print("  +------+----------+----------+-----+----------+")
    print("  |  n   | block[0] | block[1] | ... |block[29] |")
    print("  | (커밋 | (원래    | (원래    |     | (원래    |")
    print("  | 블록수)| 위치 0) | 위치 1)  |     | 위치 29) |")
    print("  +------+----------+----------+-----+----------+")
    print()
    print("  commit() 과정:")
    print("    1. write_log()  : cache -> log 영역으로 복사")
    print("    2. write_head() : header.n 기록 (커밋 포인트!)")
    print("    3. install_trans(): log -> 원래 위치로 복사")
    print("    4. write_head() : header.n = 0 (로그 삭제)")
    print()

    max_write = ((MAXOPBLOCKS - 1 - 1 - 2) // 2) * BSIZE
    print(f"  filewrite() 한 번에 쓸 수 있는 최대 크기:")
    print(f"    (MAXOPBLOCKS-1-1-2)/2 * BSIZE")
    print(f"    = ({MAXOPBLOCKS}-1-1-2)/2 * {BSIZE}")
    print(f"    = {(MAXOPBLOCKS - 1 - 1 - 2) // 2} * {BSIZE}")
    print(f"    = {max_write} bytes")
    print()


def print_directory_structure():
    """
    디렉토리 엔트리 구조 시각화

    xv6에서 디렉토리는 특별한 파일입니다:
    - type이 T_DIR(1)인 inode
    - 데이터 블록에 struct dirent 배열이 저장됨
    - 각 dirent는 (inode번호, 파일이름) 쌍으로 이름->inode 매핑 제공
    - dirlookup(): 디렉토리의 데이터를 순회하며 이름으로 inode를 검색
    - dirlink(): 새로운 엔트리를 추가 (inum==0인 빈 슬롯을 찾아 기록)
    - 파일 이름은 최대 14자로 제한 (DIRSIZ=14)
    """
    print("[디렉토리 엔트리 구조]")
    print()
    print(f"  struct dirent {{")
    print(f"    ushort inum;       // inode 번호 (2 bytes)")
    print(f"    char   name[14];   // 파일 이름  (14 bytes)")
    print(f"  }};  // 총 {DIRENT_SIZE} bytes")
    print()
    print(f"  블록당 디렉토리 엔트리 수 = {BSIZE} / {DIRENT_SIZE}"
          f" = {BSIZE // DIRENT_SIZE}")
    print()

    print("  루트 디렉토리 (\"/\") 예시:")
    print("  +-------+----------------+")
    print("  | inum  | name           |")
    print("  +-------+----------------+")
    print("  |   1   | .              |")
    print("  |   1   | ..             |")
    print("  |   2   | console        |")
    print("  |   3   | init           |")
    print("  |   4   | sh             |")
    print("  |  ...  | ...            |")
    print("  +-------+----------------+")
    print()


def print_buffer_cache():
    """
    Buffer Cache (bio.c) 구조 시각화

    Buffer cache는 디스크 블록을 메모리에 캐시하는 계층입니다.

    핵심 API:
    - bread(dev, blockno): 블록 읽기 (캐시 히트 시 디스크 I/O 없음)
    - bwrite(buf): 버퍼 내용을 디스크에 기록
    - brelse(buf): 버퍼 사용 완료 (refcnt 감소, LRU 리스트 앞으로 이동)

    자료구조:
    - NBUF개의 struct buf로 구성된 이중 연결 리스트 (LRU 순서)
    - 각 buf: { valid, disk, dev, blockno, lock, refcnt, prev, next, data[BSIZE] }
    - bget()에서 캐시 미스 시 LRU(가장 오래된) 버퍼를 재사용

    동시성 제어:
    - bcache.lock (spinlock): 리스트 구조 변경과 refcnt 보호
    - buf.lock (sleeplock): 버퍼 데이터 보호 (디스크 I/O 중 sleep 가능)
      spinlock이 아닌 sleeplock을 사용하는 이유: 디스크 I/O는 느리므로
      다른 프로세스가 CPU를 사용할 수 있도록 sleep 가능해야 함
    """
    print("[Buffer Cache 구조]")
    print()
    print(f"  NBUF = {NBUF} (캐시할 수 있는 최대 블록 수)")
    print()
    print("  이중 연결 리스트 (LRU 순서):")
    print()
    print("   MRU (최근 사용)                      LRU (오래된)")
    print("    |                                      |")
    print("    v                                      v")
    print("  head <-> buf[A] <-> buf[B] <-> ... <-> buf[Z] <-> head")
    print("       next->                               <-prev")
    print()
    print("  brelse() : refcnt==0이면 head.next로 이동 (MRU)")
    print("  bget()   : 캐시 미스 시 head.prev부터 빈 버퍼 탐색 (LRU)")
    print()
    print("  잠금 체계:")
    print("    bcache.lock (spin-lock)  : 리스트 구조, refcnt 보호")
    print("    buf.lock    (sleep-lock) : 버퍼 데이터 보호 (I/O 중 sleep 가능)")
    print()


def main():
    """메인 함수: 각 섹션을 순서대로 출력하여 xv6 디스크 구조를 시각화합니다."""
    layout, nmeta, nblocks = calculate_layout()

    print_header()
    print_constants()

    print("-" * 70)
    print()
    print_layout_table(layout, nmeta, nblocks)

    print("-" * 70)
    print()
    print_visual_layout(layout)

    print("-" * 70)
    print()
    print_inode_structure()

    print("-" * 70)
    print()
    print_file_size_limits()

    print("-" * 70)
    print()
    print_log_structure()

    print("-" * 70)
    print()
    print_directory_structure()

    print("-" * 70)
    print()
    print_buffer_cache()

    print("=" * 70)
    print("  시각화 완료")
    print("=" * 70)


if __name__ == "__main__":
    main()
