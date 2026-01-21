#!/usr/bin/env python3
import os
import re
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# --- 1. Command Line Directory Handling ---
# Usage: python3 plot_script.py [BASE_DIR]
# Example: python3 plot_script.py ./build
# Example: python3 plot_script.py ./my_experimental_data

if len(sys.argv) > 1:
    BASE_DIR = sys.argv[1]
else:
    BASE_DIR = "results"  # Default fallback

# Define which sub-test directory this specific script targets
SUB_DIR = "test_fix_wr" 
RESULTS_DIR = os.path.join(BASE_DIR, SUB_DIR)


def parse_fixwr_files(directory):
    data = []
    total_time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    # Added regex for query count
    queries_count_re = re.compile(r"Number of queries\s+(\d+)")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame()

    for filename in os.listdir(directory):
        if not filename.endswith(".txt"):
            continue
            
        algo = "FCDS" if filename.startswith("fcds") else "Concurrent"
        workers_match = re.search(r"workers(\d+)", filename)
        queries_match = re.search(r"queries(\d+)", filename)
        
        if not workers_match or not queries_match:
            continue
            
        w_count = int(workers_match.group(1))
        q_count = int(queries_match.group(1))
        
        with open(os.path.join(directory, filename), 'r') as f:
            content = f.read()
            time_match = total_time_re.search(content)
            q_count_match = queries_count_re.search(content)
            
            if time_match and q_count_match:
                data.append({
                    "Algo": algo,
                    "Workers": w_count,
                    "Queries": q_count,
                    "TotalTime": float(time_match.group(1)),
                    "ActualQueries": int(q_count_match.group(1))
                })
                
    return pd.DataFrame(data)

# 1. Load data
df = parse_fixwr_files(RESULTS_DIR)

if df.empty:
    print("No data found. Check your RESULTS_DIR path.")
else:
    # 2. Aggregation
    summary = df.groupby(["Algo", "Workers", "Queries"]).agg({
        "TotalTime": ["mean", "std"],
        "ActualQueries": "mean"
    }).reset_index()
    summary.columns = ["Algo", "Workers", "Queries", "AvgTime", "StdTime", "AvgQueries"]
    summary["StdTime"] = summary["StdTime"].fillna(0)

    # 3. Normalization: Queries / Time
    summary["NormalizedQueries"] = summary["AvgQueries"] / summary["AvgTime"]

    plt.style.use('seaborn-v0_8-muted')

    # Create figure with two subplots side-by-side
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 7), dpi=120)

    # Prepare Shared X-Axis labels (Sorted by Workers)
    all_configs = summary[["Workers", "Queries"]].drop_duplicates().sort_values("Workers")
    all_workers = all_configs["Workers"].values
    labels = [f"{w}w / {q}q" for w, q in zip(all_configs["Workers"], all_configs["Queries"])]
    x_indices = np.arange(len(labels))

    # ==========================================
    # LEFT PLOT: Your Original Ratio Plot
    # ==========================================
    for algo, color, marker in [("FCDS", "#1f77b4", "o"), ("Concurrent", "#d62728", "s")]:
        subset = summary[summary["Algo"] == algo].sort_values("Workers")
        if subset.empty: continue
        
        ax1.fill_between(subset["Workers"], 
                         subset["AvgTime"] - subset["StdTime"], 
                         subset["AvgTime"] + subset["StdTime"], 
                         color=color, alpha=0.15)
        
        ax1.plot(subset["Workers"], subset["AvgTime"], 
                 color=color, marker=marker, markersize=9, 
                 linewidth=2.5, label=algo, markeredgecolor='white')

    ax1.set_xticks(all_workers)
    ax1.set_xticklabels(labels)
    ax1.set_title("Fixed Load Performance: Execution Time", fontsize=14, fontweight='bold')
    ax1.set_xlabel("Thread Distribution (Workers / Queries)", fontsize=12)
    ax1.set_ylabel("Total Execution Time (ms)", fontsize=12)
    ax1.grid(True, ls="--", alpha=0.6)
    ax1.legend(frameon=True, facecolor='white')

    # ==========================================
    # RIGHT PLOT: Normalized Query Histograms
    # ==========================================
    bar_width = 0.35
    
    # Pivot data to ensure alignment even if an algo is missing a data point
    pivot_df = summary.pivot(index="Workers", columns="Algo", values="NormalizedQueries").reindex(all_workers).fillna(0)
    
    fcds_vals = pivot_df["FCDS"] if "FCDS" in pivot_df.columns else np.zeros(len(all_workers))
    conc_vals = pivot_df["Concurrent"] if "Concurrent" in pivot_df.columns else np.zeros(len(all_workers))

    ax2.bar(x_indices - bar_width/2, fcds_vals, bar_width, label='FCDS', color="#1f77b4", alpha=0.8)
    ax2.bar(x_indices + bar_width/2, conc_vals, bar_width, label='Concurrent', color="#d62728", alpha=0.8)

    ax2.set_title("Normalized Throughput: Queries / ms", fontsize=14, fontweight='bold')
    ax2.set_xlabel("Thread Distribution (Workers / Queries)", fontsize=12)
    ax2.set_ylabel("Avg Queries per ms", fontsize=12)
    ax2.set_xticks(x_indices)
    ax2.set_xticklabels(labels)
    ax2.grid(True, axis='y', ls="--", alpha=0.6)
    ax2.legend(frameon=True, facecolor='white')

    plt.tight_layout()
    plt.savefig("plot_fixed_writes_combined.png")
    print("Saved combined plot: plot_fixed_writes_combined.png")
    plt.show()