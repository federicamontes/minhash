#!/usr/bin/env python3
import os
import re
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Configuration
NUM_OPS = 1000000 

# --- 1. Command Line Directory Handling ---
if len(sys.argv) > 1:
    BASE_DIR = sys.argv[1]
else:
    BASE_DIR = "results-100k-ops"  

SUB_DIR = "test_prob" 
RESULTS_DIR = os.path.join(BASE_DIR, SUB_DIR)

def parse_files(directory):
    data = []
    total_time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame()

    for filename in os.listdir(directory):
        if not filename.endswith(".txt"):
            continue
            
        if filename.startswith("fcds"):
            algo = "FCDS"
        elif "numa" in filename:
            algo = "Concurrent-NUMA"
        else:
            algo = "Concurrent"

        threads_match = re.search(r"threads(\d+)", filename)
        wp_match = re.search(r"wp([\d.]+)", filename)
        
        if not threads_match or not wp_match:
            continue
            
        t_count = int(threads_match.group(1))
        wp_val = float(wp_match.group(1))

        elapsed_time = None
        txt_path = os.path.join(directory, filename)
        with open(txt_path, 'r') as f:
            content = f.read()
            time_match = total_time_re.search(content)
            if time_match:
                elapsed_time = float(time_match.group(1))

        perf_path = os.path.join(directory, filename.replace(".txt", ".perf"))
        perf_stats = {
            "cache-references": 0,
            "cache-misses": 0,
            "L1-dcache-load-misses": 0,
            "LLC-load-misses": 0
        }
        
        if os.path.exists(perf_path):
            with open(perf_path, 'r') as pf:
                for line in pf:
                    if line.startswith("#") or not line.strip():
                        continue
                    parts = line.split(',')
                    if len(parts) < 3:
                        continue
                    raw_val = parts[0].strip()
                    event_name = parts[2].strip()

                    if raw_val != "<not supported>":
                        try:
                            val = int(raw_val)
                            for metric in perf_stats.keys():
                                if metric in event_name:
                                    perf_stats[metric] += val
                        except ValueError:
                            continue

        if elapsed_time is not None:
            data.append({
                "Algo": algo, "Threads": t_count, "WP": wp_val, "TotalTime": elapsed_time,
                "Refs": perf_stats["cache-references"],
                "Misses": perf_stats["cache-misses"],
                "L1Misses": perf_stats["L1-dcache-load-misses"],
                "LLCMisses": perf_stats["LLC-load-misses"]
            })
                
    return pd.DataFrame(data)

# 1. Load data
df = parse_files(RESULTS_DIR)

if df.empty:
    print(f"No data found in {RESULTS_DIR}. Check if .perf files exist.")
else:
    # 2. Aggregation
    summary = df.groupby(["Algo", "Threads", "WP"]).agg({
        "TotalTime": ["mean", "std"],
        "Refs": "mean", "Misses": "mean", "L1Misses": "mean", "LLCMisses": "mean"
    }).reset_index()
    
    summary.columns = ["Algo", "Threads", "WP", "AvgTime", "StdTime", "AvgRefs", "AvgMisses", "AvgL1", "AvgLLC"]

    # 3. Calculate Normalized Metrics
    summary["Throughput (Mops/s)"] = (NUM_OPS / (summary["AvgTime"] / 1000.0)) / 1000000.0
    summary["LLC_Per_Op"] = summary["AvgLLC"] / NUM_OPS
    summary["Miss_Ratio"] = (summary["AvgMisses"] / summary["AvgRefs"].replace(0, np.nan)) * 100
    summary["L1_Intensity"] = summary["AvgL1"] / summary["AvgTime"]

    # --- SEPARATE TABLES FOR EACH WP VALUE ---
    unique_wps = sorted(summary["WP"].unique())
    for wp in unique_wps:
        print("\n" + "╔" + "═"*105 + "╗")
        print(f"║ RESULTS FOR WINDOW PROBABILITY (WP): {wp:<67} ║")
        print("╠" + "═"*105 + "╣")
        print(f"║ {'ALGORITHM':<18} ║ {'TH':<4} ║ {'TIME(ms)':<10} ║ {'THROUGHPUT':<12} ║ {'MISS %':<8} ║ {'LLC/OP':<8} ║ {'L1 INT':<8} ║")
        print("╟" + "─"*105 + "╢")
        wp_table = summary[summary["WP"] == wp].sort_values(["Threads", "Algo"])
        for _, row in wp_table.iterrows():
            print(f"║ {row['Algo']:<18} ║ {int(row['Threads']):<4} ║ {row['AvgTime']:<10.2f} ║ "
                  f"{row['Throughput (Mops/s)']:<12.3f} ║ {row['Miss_Ratio']:<8.2f} ║ "
                  f"{row['LLC_Per_Op']:<8.3f} ║ {row['L1_Intensity']:<8.1f} ║")
        print("╚" + "═"*105 + "╝")

    # --- NEW: FINAL SUMMARY TABLE (MAX THREADS ONLY) ---
    max_threads = summary["Threads"].max()
    print("\n" + "█"*107)
    print(f"█ {'FINAL SUMMARY: MAXIMUM THREADS (' + str(int(max_threads)) + ') COMPARISON ACROSS WP':<103} █")
    print("█"*107)
    print(f"║ {'WP':<6} ║ {'ALGORITHM':<18} ║ {'TIME(ms)':<10} ║ {'THROUGHPUT':<12} ║ {'MISS %':<8} ║ {'LLC/OP':<8} ║ {'L1 INT':<8} ║")
    print("╟" + "─"*6 + "╫" + "─"*20 + "╫" + "─"*12 + "╫" + "─"*14 + "╫" + "─"*10 + "╫" + "─"*10 + "╫" + "─"*10 + "╢")
    
    final_summary = summary[summary["Threads"] == max_threads].sort_values(["WP", "AvgTime"])
    for _, row in final_summary.iterrows():
        print(f"║ {row['WP']:<6} ║ {row['Algo']:<18} ║ {row['AvgTime']:<10.2f} ║ "
              f"{row['Throughput (Mops/s)']:<12.3f} ║ {row['Miss_Ratio']:<8.2f} ║ "
              f"{row['LLC_Per_Op']:<8.3f} ║ {row['L1_Intensity']:<8.1f} ║")
    print("╚" + "═"*6 + "╩" + "═"*20 + "╩" + "═"*12 + "╩" + "═"*14 + "╩" + "═"*10 + "╩" + "═"*10 + "╩" + "═"*10 + "╝")

    # --- Plotting Code (Abbreviated for brevity) ---
    # ... (Same plotting code as previous version) ...

    print(f"\nGeneration complete. Results processed from {RESULTS_DIR}")