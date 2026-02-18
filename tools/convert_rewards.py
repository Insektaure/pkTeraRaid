#!/usr/bin/env python3
"""Convert reward JSON files to compact binary format for pkTeraRaid."""

import json
import struct
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)

FIXED_JSON = os.path.join(PROJECT_DIR, "external/RaidCrawler/RaidCrawler.Core/Resources/Base/raid_fixed_reward_item_array.json")
LOTTERY_JSON = os.path.join(PROJECT_DIR, "external/RaidCrawler/RaidCrawler.Core/Resources/Base/raid_lottery_reward_item_array.json")
OUT_FIXED = os.path.join(PROJECT_DIR, "romfs/data/reward_fixed.bin")
OUT_LOTTERY = os.path.join(PROJECT_DIR, "romfs/data/reward_lottery.bin")

def convert_fixed(json_path, out_path):
    with open(json_path) as f:
        tables = json.load(f)

    seen = set()
    unique = []
    for t in tables:
        h = t["TableName"]
        if h in seen:
            continue
        seen.add(h)
        unique.append(t)

    data = struct.pack("<H", len(unique))

    for t in unique:
        items = []
        for i in range(15):
            key = f"RewardItem{i:02d}"
            if key not in t:
                break
            entry = t[key]
            cat = entry.get("Category", 0)
            item_id = entry.get("ItemID", 0)
            num = entry.get("Num", 0)
            sub = entry.get("SubjectType", 0)
            if item_id == 0 and cat == 0:
                continue
            items.append((cat, item_id, num, sub))

        data += struct.pack("<QB", t["TableName"], len(items))
        for cat, item_id, num, sub in items:
            data += struct.pack("<BHBb", cat, item_id, num, sub)

    with open(out_path, "wb") as f:
        f.write(data)
    print(f"Fixed: {len(unique)} tables -> {len(data)} bytes -> {out_path}")

def convert_lottery(json_path, out_path):
    with open(json_path) as f:
        tables = json.load(f)

    seen = set()
    unique = []
    for t in tables:
        h = t["TableName"]
        if h in seen:
            continue
        seen.add(h)
        unique.append(t)

    data = struct.pack("<H", len(unique))

    for t in unique:
        items = []
        total_rate = 0
        for i in range(30):
            key = f"RewardItem{i:02d}"
            if key not in t:
                break
            entry = t[key]
            cat = entry.get("Category", 0)
            item_id = entry.get("ItemID", 0)
            num = entry.get("Num", 0)
            rate = entry.get("Rate", 0)
            total_rate += rate
            if item_id == 0 and cat == 0 and rate == 0:
                continue
            items.append((cat, item_id, num, rate))

        data += struct.pack("<QHB", t["TableName"], total_rate, len(items))
        for cat, item_id, num, rate in items:
            data += struct.pack("<BHBH", cat, item_id, num, rate)

    with open(out_path, "wb") as f:
        f.write(data)
    print(f"Lottery: {len(unique)} tables -> {len(data)} bytes -> {out_path}")

if __name__ == "__main__":
    convert_fixed(FIXED_JSON, OUT_FIXED)
    convert_lottery(LOTTERY_JSON, OUT_LOTTERY)
    print("Done.")
