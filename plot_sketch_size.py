#!/usr/bin/env python3
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Configuration
RESULTS_DIR = "build/test_sketch_size/"
NUM_OPS = 100000 

def parse_files(directory):
    data = []
    total_time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame()

    for filename in os.listdir(directory):
        if not filename.endswith(".txt"):
            continue
            
        algo = "FCDS" if filename.startswith("fcds") else "Concurrent"
        # Updated Regex to find 'sz' instead of 'threads' for the X-axis
        size_match = re.search(r"sz(\d+)", filename)
        wp_match = re.search(r"wp([\d.]+)", filename)
        threads_match = re.search(r"threads(\d+)", filename)
        
        if not size_match or not wp_match:
            continue
            
        sketch_size = int(size_match.group(1))
        wp_val = float(wp_match.group(1))
        t_count = int(threads_match.group(1)) if threads_match else "Max"

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
        perf_stats = {"cache-references": 0, "cache-misses": 0, 
                      "L1-dcache-load-misses": 0, "LLC-load-misses": 0}
        
        if os.path.exists(perf_path):
            with open(perf_path, 'r') as pf:
                for line in pf:
                    if line.startswith("#") or not line.strip(): continue
                    parts = line.split(',')
                    if len(parts) < 3: continue
                    raw_val = parts[0].strip()
                    event_name = parts[2].strip()

                    if raw_val != "<not supported>":
                        val = int(raw_val)
                        for metric in perf_stats.keys():
                            if metric in event_name:
                                perf_stats[metric] += val

        if elapsed_time is not None:
            data.append({
                "Algo": algo, "Size": sketch_size, "WP": wp_val, "Threads": t_count,
                "TotalTime": elapsed_time,
                "Refs": perf_stats["cache-references"],
                "Misses": perf_stats["cache-misses"],
                "L1Misses": perf_stats["L1-dcache-load-misses"],
                "LLCMisses": perf_stats["LLC-load-misses"]
            })
                
    return pd.DataFrame(data)

# 1. Load data
df = parse_files(RESULTS_DIR)

if df.empty:
    print("No data found. Check your directory and filenames.")
else:
    # 2. Aggregation
    summary = df.groupby(["Algo", "Size", "WP", "Threads"]).agg({
        "TotalTime": ["mean", "std"],
        "Refs": "mean", "Misses": "mean", "L1Misses": "mean", "LLCMisses": "mean"
    }).reset_index()
    
    summary.columns = ["Algo", "Size", "WP", "Threads", "AvgTime", "StdTime", "AvgRefs", "AvgMisses", "AvgL1", "AvgLLC"]

    # 3. Metrics
    summary["LLC_Per_Op"] = summary["AvgLLC"] / NUM_OPS
    summary["Miss_Ratio"] = (summary["AvgMisses"] / summary["AvgRefs"].replace(0, np.nan)) * 100
    summary["L1_Intensity"] = summary["AvgL1"] / summary["AvgTime"]

    plt.style.use('seaborn-v0_8-muted')
    
    for wp in sorted(summary["WP"].unique()):
        wp_df = summary[summary["WP"] == wp]
        all_sizes = sorted(wp_df["Size"].unique())
        x_indices = np.arange(len(all_sizes))
        bar_width = 0.35
        t_used = wp_df["Threads"].iloc[0]

        # --- PLOT 1: RUNTIME PERFORMANCE (Line Plot vs Sketch Size) ---
        plt.figure(figsize=(10, 5), dpi=100)
        for algo, color, marker in [("FCDS", "#1f77b4", "o"), ("Concurrent", "#d62728", "s")]:
            subset = wp_df[wp_df["Algo"] == algo].sort_values("Size")
            plt.fill_between(subset["Size"], subset["AvgTime"] - subset["StdTime"], 
                             subset["AvgTime"] + subset["StdTime"], color=color, alpha=0.15)
            plt.plot(subset["Size"], subset["AvgTime"], color=color, marker=marker, linewidth=2, label=algo)
        
        plt.title(f"Scaling Sketch Size (Write Prob: {wp}, Threads: {t_used})", fontweight='bold')
        plt.xlabel("Sketch Size (Number of Elements)")
        plt.ylabel("Execution Time (ms)")
        plt.grid(True, alpha=0.3)
        plt.legend()
        plt.savefig(f"runtime_size_wp_{wp}.png")
        plt.close()

        # --- PLOT 2: CACHE ANALYSIS DASHBOARD (Bar Plot vs Sketch Size) ---
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6), dpi=100)
        
        metrics_to_plot = [
            ("L1_Intensity", "L1-dcache Miss Intensity", "Misses / ms", ax1),
            ("Miss_Ratio", "Total Cache Miss Ratio", "Miss %", ax2),
            ("LLC_Per_Op", "LLC Misses per Operation", "Misses / Op", ax3)
        ]

        for col_name, title, ylabel, ax in metrics_to_plot:
            pivot = wp_df.pivot(index="Size", columns="Algo", values=col_name).reindex(all_sizes).fillna(0)
            fcds_vals = pivot["FCDS"] if "FCDS" in pivot.columns else [0]*len(all_sizes)
            conc_vals = pivot["Concurrent"] if "Concurrent" in pivot.columns else [0]*len(all_sizes)

            ax.bar(x_indices - bar_width/2, fcds_vals, bar_width, label='FCDS', color="#1f77b4", edgecolor='white')
            ax.bar(x_indices + bar_width/2, conc_vals, bar_width, label='Concurrent', color="#d62728", edgecolor='white')

            ax.set_title(title, fontweight='bold', pad=10)
            ax.set_ylabel(ylabel)
            ax.set_xticks(x_indices)
            ax.set_xticklabels(all_sizes)
            ax.grid(True, axis='y', alpha=0.3, linestyle='--')
            if ax == ax3: ax.legend()

        plt.suptitle(f"Cache Performance vs Sketch Size (WP: {wp}, Threads: {t_used})", fontsize=16, fontweight='bold')
        plt.tight_layout(rect=[0, 0.03, 1, 0.95])
        plt.savefig(f"cache_size_analysis_wp_{wp}.png", bbox_inches='tight')
        plt.show()

    print("Generation complete. X-axis is now Sketch Size.")
