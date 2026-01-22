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
    BASE_DIR = "results-100k-ops"  # Kept as per your original script

# Keeping your SUB_DIR naming exactly the same
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
            
        # --- NEW PARSING LOGIC FOR 3 CURVES ---
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

        # 1. Extract Elapsed Time
        elapsed_time = None
        txt_path = os.path.join(directory, filename)
        with open(txt_path, 'r') as f:
            content = f.read()
            time_match = total_time_re.search(content)
            if time_match:
                elapsed_time = float(time_match.group(1))

        # 2. Extract and SUM Hybrid Perf Events
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
                        val = int(raw_val)
                        for metric in perf_stats.keys():
                            if metric in event_name:
                                perf_stats[metric] += val

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
    summary["LLC_Per_Op"] = summary["AvgLLC"] / NUM_OPS
    summary["Miss_Ratio"] = (summary["AvgMisses"] / summary["AvgRefs"].replace(0, np.nan)) * 100
    summary["L1_Intensity"] = summary["AvgL1"] / summary["AvgTime"]

    plt.style.use('seaborn-v0_8-muted')
    
    # Configuration for the 3 variants
    ALGO_STYLES = [
        ("FCDS", "#1f77b4", "o"), 
        ("Concurrent", "#d62728", "s"),
        ("Concurrent-NUMA", "#2ca02c", "D")
    ]

    for wp in sorted(summary["WP"].unique()):
        wp_df = summary[summary["WP"] == wp]
        all_threads = sorted(wp_df["Threads"].unique())
        x_indices = np.arange(len(all_threads))
        bar_width = 0.25 # Smaller width to fit 3 bars

        # --- PLOT 1: RUNTIME PERFORMANCE (3 Curves) ---
        plt.figure(figsize=(10, 5), dpi=100)
        for algo, color, marker in ALGO_STYLES:
            subset = wp_df[wp_df["Algo"] == algo].sort_values("Threads")
            if subset.empty: continue
            plt.fill_between(subset["Threads"], subset["AvgTime"] - subset["StdTime"], 
                             subset["AvgTime"] + subset["StdTime"], color=color, alpha=0.1)
            plt.plot(subset["Threads"], subset["AvgTime"], color=color, marker=marker, 
                     linewidth=2, label=algo)
        
        plt.title(f"Runtime Performance Comparison (WP: {wp})")
        plt.xlabel("Threads")
        plt.ylabel("Execution Time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.savefig(f"runtime_wp_{wp}.png")
        plt.close()

        # --- PLOT 2: PERF ANALYSIS DASHBOARD (3 Histograms) ---
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(20, 6), dpi=100)
        
        metrics_to_plot = [
            ("LLC_Per_Op", "LLC Misses per Operation", "Misses / Op", ax1),
            ("Miss_Ratio", "Total Cache Miss Ratio", "Miss %", ax2),
            ("L1_Intensity", "L1-dcache Miss Intensity", "Misses / ms", ax3)
        ]

        

        for col_name, title, ylabel, ax in metrics_to_plot:
            pivot = wp_df.pivot(index="Threads", columns="Algo", values=col_name).reindex(all_threads).fillna(0)
            
            # Position the three bars side-by-side
            for i, (algo, color, _) in enumerate(ALGO_STYLES):
                if algo in pivot.columns:
                    ax.bar(x_indices + (i - 1) * bar_width, pivot[algo], bar_width, 
                           label=algo, color=color, edgecolor='white', linewidth=0.5)

            ax.set_title(title, fontweight='bold', pad=10)
            ax.set_ylabel(ylabel)
            ax.set_xticks(x_indices)
            ax.set_xticklabels(all_threads)
            ax.grid(True, axis='y', alpha=0.3, linestyle='--')
            if ax == ax3: 
                ax.legend(loc='upper right')

        plt.suptitle(f"Multi-Variant Cache Analysis (WP: {wp})", fontsize=16, fontweight='bold')
        plt.tight_layout(rect=[0, 0.03, 1, 0.95])
        plt.savefig(f"cache_analysis_wp_{wp}.png", bbox_inches='tight')
        plt.show()

    print(f"Generation complete. Results processed from {RESULTS_DIR}")