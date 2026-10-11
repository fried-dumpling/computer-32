# -*- coding: utf-8 -*-
"""
ATLI5.csv(비정상 동작 로그) vs ATLI5C.csv(정상 동작 로그) 비교.

같은 스텝(step) 기준으로 행을 정렬 비교해서, 두 로그가 처음으로 갈라지는
지점(컬럼/값)을 찾고, 이후 어떤 컬럼들이 연쇄적으로 틀어지는지 집계한다.

사용법:
    python compare_atli5.py
"""
import csv
import sys
import os

BASE = os.path.dirname(os.path.abspath(__file__))
BAD_FILE = os.path.join(BASE, "ATLI5.csv")     # 비정상 동작
GOOD_FILE = os.path.join(BASE, "ATLI5C.csv")   # 정상 동작

CONTEXT_COLS = ["step", "cycle", "com1_inst", "com2_inst", "R", "clr",
                "FL_MEM", "FL_JPU", "FL_ECLR", "FL_INT"]


def load(fname):
    with open(fname, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.reader(f))
    return rows[0], rows[1:]


def getName():
    global GOOD_FILE, BAD_FILE
    name_good = input("good file: ")
    name_bad = input("bad file: ")
    GOOD_FILE = os.path.join(BASE, name_good)
    BAD_FILE = os.path.join(BASE, name_bad)

def main():
    while True:
        getName()
        try:
            h_bad, d_bad = load(BAD_FILE)
            h_good, d_good = load(GOOD_FILE)
            break
        except Exception as e:
            print(e)

    print(f"{os.path.basename(BAD_FILE)}  (비정상): {len(d_bad)} rows")
    print(f"{os.path.basename(GOOD_FILE)} (정상)  : {len(d_good)} rows")

    if h_bad != h_good:
        print("\n!! 헤더가 다릅니다 !!")
        for i, (a, b) in enumerate(zip(h_bad, h_good)):
            if a != b:
                print(f"  col {i}: {a!r} vs {b!r}")
        sys.exit(1)

    header = h_bad
    idx = {name: i for i, name in enumerate(header)}
    n = min(len(d_bad), len(d_good))

    # 1) 처음으로 값이 갈라지는 행 찾기
    first_diff = None
    for i in range(n):
        if d_bad[i] != d_good[i]:
            first_diff = i
            break

    if first_diff is None:
        print(f"\n처음 {n}행까지는 완전히 동일합니다. (그 이후 길이만 다름)")
        return

    row_bad, row_good = d_bad[first_diff], d_good[first_diff]
    diff_cols = [(j, header[j], row_bad[j], row_good[j])
                 for j in range(len(header)) if row_bad[j] != row_good[j]]

    print(f"\n=== 첫 분기 지점: 데이터 행 index {first_diff} "
          f"(step={row_bad[idx['step']]}, cycle={row_bad[idx['cycle']]}) ===")
    for j, name, vb, vg in diff_cols:
        print(f"  [{j:3d}] {name:20s}: 비정상={vb!r:12s} 정상={vg!r}")

    print(f"\n--- 직전 일치 행 (index {first_diff - 1}) 주요 컨텍스트 ---")
    prev = d_bad[first_diff - 1]
    for name in CONTEXT_COLS:
        if name in idx:
            print(f"  {name:12s}: {prev[idx[name]]}")

    # 2) 전체 구간에서 값이 갈렸던 컬럼별 집계 (연쇄 파급 확인용)
    diff_counts = {}
    first_seen = {}
    for i in range(first_diff, n):
        rb, rg = d_bad[i], d_good[i]
        for j, name in enumerate(header):
            if rb[j] != rg[j]:
                diff_counts[name] = diff_counts.get(name, 0) + 1
                first_seen.setdefault(name, i)

    print(f"\n=== 분기 이후 값이 달랐던 컬럼 (등장 빈도순, 상위 15개) ===")
    for name, cnt in sorted(diff_counts.items(), key=lambda x: -x[1])[:15]:
        print(f"  {name:20s}: {cnt:5d}회, 최초 발생 행={first_seen[name]}")

    if len(d_bad) != len(d_good):
        print(f"\n비고: 로그 총 길이도 다릅니다 "
              f"(비정상={len(d_bad)}행, 정상={len(d_good)}행)")


if __name__ == "__main__":
    main()
