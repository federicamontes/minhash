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
    BASE_DIR = "results-1M-ops"  

SUB_DIR = "test_prob" 
RESULTS_DIR = os.path.join(BASE_DIR, SUB_DIR)

def parse_files(directory):
    data = []
    serial_results = []
    
    # Regex Patterns
    total_time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    serial_time_re = re.compile(r"Elapsed time:\s+([\d.]+)\s+ms")
    # Updated WP regex to avoid trailing dots/punctuation
    wp_regex = re.compile(r"wp([\d.]+)")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame(), {}

    all_files = os.listdir(directory)
    print(f"Scanning {len(all_files)} files in {directory}...")

    for filename in all_files:
        wp_match = wp_regex.search(filename)
        if not wp_match:
            continue
            
        # Clean the string: remove a trailing dot if the regex captured it
        wp_str = wp_match.group(1).rstrip('.')
        try:
            wp_val = float(wp_str)
        except ValueError:
            print(f"  [Skip] Could not parse WP from filename: {filename}")
            continue

        txt_path = os.path.join(directory, filename)

        # --- CASE 1: SERIAL ---
        if "serial" in filename.lower():
            try:
                with open(txt_path, 'r') as f:
                    content = f.read()
                    s_match = serial_time_re.search(content)
                    if s_match:
                        val = float(s_match.group(1))
                        serial_results.append({"WP": wp_val, "Time": val})
            except: pass
            continue

        # --- CASE 2: CONCURRENT ---
        if not filename.endswith(".txt"):
            continue

        if filename.startswith("fcds"):
            algo = "FCDS"
        elif "numa" in filename:
            algo = "Concurrent-NUMA"
        else:
            algo = "Concurrent"

        threads_match = re.search(r"threads(\d+)", filename)
        if not threads_match:
            continue
            
        t_count = int(threads_match.group(1))

        elapsed_time = None
        try:
            with open(txt_path, 'r') as f:
                content = f.read()
                time_match = total_time_re.search(content)
                if time_match:
                    elapsed_time = float(time_match.group(1))
        except: continue

        # Perf Stats Parsing
        perf_path = os.path.join(directory, filename.replace(".txt", ".perf"))
        perf_stats = {"cache-references": 0, "cache-misses": 0, "L1-dcache-load-misses": 0, "LLC-load-misses": 0}
        
        if os.path.exists(perf_path):
            try:
                with open(perf_path, 'r') as pf:
                    for line in pf:
                        if line.startswith("#") or not line.strip(): continue
                        parts = line.split(',')
                        if len(parts) < 3: continue
                        raw_val = parts[0].strip()
                        event_name = parts[2].strip()
                        if raw_val != "<not supported>":
                            try:
                                val = int(raw_val)
                                for metric in perf_stats.keys():
                                    if metric in event_name: perf_stats[metric] += val
                            except ValueError: continue
            except: pass

        if elapsed_time is not None:
            data.append({
                "Algo": algo, "Threads": t_count, "WP": wp_val, "TotalTime": elapsed_time,
                "Refs": perf_stats["cache-references"],
                "Misses": perf_stats["cache-misses"],
                "L1Misses": perf_stats["L1-dcache-load-misses"],
                "LLCMisses": perf_stats["LLC-load-misses"]
            })
                
    ser_df = pd.DataFrame(serial_results)
    serial_map = ser_df.groupby("WP")["Time"].mean().to_dict() if not ser_df.empty else {}
    
    return pd.DataFrame(data), serial_map

# 1. Load data
df, serial_baseline_map = parse_files(RESULTS_DIR)

if df.empty:
    print(f"No concurrent data found in {RESULTS_DIR}.")
    sys.exit(1)

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

unique_wps = sorted(summary["WP"].unique())

# --- 4. TABLES FOR EACH WP VALUE ---
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

# --- 5. SCALABILITY SUMMARY TABLE ---
max_threads = summary["Threads"].max()
print("\n" + "╔" + "═"*120 + "╗")
print(f"║ SCALABILITY SUMMARY: MAX THREADS ({max_threads}) ACROSS ALL WP {' ':<42} ║")
print("╠" + "═"*120 + "╣")
print(f"║ {'WP':<6} ║ {'ALGORITHM':<18} ║ {'THROUGHPUT':<12} ║ {'MISS %':<8} ║ {'LLC/OP':<8} ║ {'SPD UP(REL)*':<13} ║ {'SPD UP(SER)':<11} ║")
print("╟" + "─"*120 + "╢")

max_thread_df = summary[summary["Threads"] == max_threads].sort_values(["WP", "Algo"])

for wp in unique_wps:
    wp_max = max_thread_df[max_thread_df["WP"] == wp]
    if wp_max.empty: continue
    base_throughput = wp_max["Throughput (Mops/s)"].iloc[0]
    serial_time = serial_baseline_map.get(wp, None)
    
    for _, row in wp_max.iterrows():
        rel_speedup = row['Throughput (Mops/s)'] / base_throughput
        ser_speedup_str = f"{serial_time / row['AvgTime']:<11.2f}" if (serial_time and row['AvgTime'] > 0) else f"{'N/A':<11}"

        print(f"║ {row['WP']:<6.2f} ║ {row['Algo']:<18} ║ {row['Throughput (Mops/s)']:<12.3f} ║ "
              f"{row['Miss_Ratio']:<8.2f} ║ {row['LLC_Per_Op']:<8.3f} ║ {rel_speedup:<13.2f} ║ {ser_speedup_str} ║")
    if wp != unique_wps[-1]:
        print("╟" + "─"*120 + "╢")

print("╚" + "═"*120 + "╝")

# --- 6. PLOTTING ---
plt.style.use('seaborn-v0_8-muted')
ALGO_STYLES = [
    ("FCDS", "#1f77b4", "o"), 
    ("Concurrent", "#d62728", "s"),
    ("Concurrent-NUMA", "#2ca02c", "D")
]

for wp in unique_wps:
    wp_df = summary[summary["WP"] == wp]
    all_threads = sorted(wp_df["Threads"].unique())
    x_indices = np.arange(len(all_threads))
    bar_width = 0.25

    plt.figure(figsize=(10, 5), dpi=100)
    
    # Plot Serial Line
    if wp in serial_baseline_map:
        val = serial_baseline_map[wp]
        plt.axhline(y=val, color='black', linestyle=':', linewidth=2, label=f'Serial Baseline ({val:.1f}ms)', zorder=2)

    for algo, color, marker in ALGO_STYLES:
        subset = wp_df[wp_df["Algo"] == algo].sort_values("Threads")
        if subset.empty: continue
        plt.fill_between(subset["Threads"], subset["AvgTime"] - subset["StdTime"], 
                         subset["AvgTime"] + subset["StdTime"], color=color, alpha=0.1)
        plt.plot(subset["Threads"], subset["AvgTime"], color=color, marker=marker, 
                linewidth=2, label=algo, zorder=3)
    
    plt.title(f"Runtime Performance Comparison (WP: {wp})")
    plt.xlabel("Threads")
    plt.ylabel("Execution Time (ms)")
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.savefig(f"runtime_wp_{wp}.png")
    plt.close()

    # Dashboard
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(20, 6), dpi=100)
    metrics_to_plot = [
        ("LLC_Per_Op", "LLC Misses per Operation", "Misses / Op", ax1),
        ("Miss_Ratio", "Total Cache Miss Ratio", "Miss %", ax2),
        ("L1_Intensity", "L1-dcache Miss Intensity", "Misses / ms", ax3)
    ]

    for col_name, title, ylabel, ax in metrics_to_plot:
        pivot = wp_df.pivot(index="Threads", columns="Algo", values=col_name).reindex(all_threads).fillna(0)
        for i, (algo, color, _) in enumerate(ALGO_STYLES):
            if algo in pivot.columns:
                ax.bar(x_indices + (i - 1) * bar_width, pivot[algo], bar_width, 
                       label=algo, color=color, edgecolor='white', linewidth=0.5)
        ax.set_title(title, fontweight='bold', pad=10)
        ax.set_ylabel(ylabel)
        ax.set_xticks(x_indices)
        ax.set_xticklabels(all_threads)
        ax.grid(True, axis='y', alpha=0.3, linestyle='--')
        if ax == ax3: ax.legend(loc='upper right')

    plt.suptitle(f"Multi-Variant Cache Analysis (WP: {wp})", fontsize=16, fontweight='bold')
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(f"cache_analysis_wp_{wp}.png")

print(f"\nProcessing complete. Check for 'N/A' in table if serial files are missing.")