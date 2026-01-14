#!/usr/bin/env python3
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Path to the specific results directory
RESULTS_DIR = "build/test_fix_qr/"

def parse_fixqr_files(directory):
    data = []
    time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    ins_re = re.compile(r"Number of insertions\s+(\d+)")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame()

    for filename in os.listdir(directory):
        if not filename.endswith(".txt"):
            continue
            
        algo = "FCDS" if filename.startswith("fcds") else "Concurrent"
        w_match = re.search(r"workers(\d+)", filename)
        q_match = re.search(r"queries(\d+)", filename)
        
        if not w_match or not q_match:
            continue
            
        with open(os.path.join(directory, filename), 'r') as f:
            content = f.read()
            t_match = time_re.search(content)
            i_match = ins_re.search(content)
            
            if t_match and i_match:
                data.append({
                    "Algo": algo,
                    "Workers": int(w_match.group(1)),
                    "Queries": int(q_match.group(1)),
                    "TotalTime": float(t_match.group(1)),
                    "Insertions": int(i_match.group(1))
                })
                
    return pd.DataFrame(data)

# 1. Load data
df = parse_fixqr_files(RESULTS_DIR)

if df.empty:
    print("No data found.")
else:
    # 2. Aggregation: Average values per configuration
    summary = df.groupby(["Algo", "Workers", "Queries"]).agg({
        "TotalTime": ["mean", "std"],
        "Insertions": "mean"
    }).reset_index()
    summary.columns = ["Algo", "Workers", "Queries", "AvgTime", "StdTime", "AvgIns"]
    summary["StdTime"] = summary["StdTime"].fillna(0)

    # 3. Normalization: Calculate insertions per millisecond (Throughput)
    summary["NormalizedIns"] = summary["AvgIns"] / summary["AvgTime"]

    # 4. Align data using pivot to prevent broadcasting errors
    pivot_norm = summary.pivot(index=["Queries", "Workers"], columns="Algo", values="NormalizedIns").fillna(0).reset_index()
    pivot_norm = pivot_norm.sort_values("Queries")

    # 5. Setup Figure
    plt.style.use('seaborn-v0_8-muted')
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 7), dpi=120)
    
    # Master X-axis data
    q_vals = pivot_norm["Queries"].values
    x_labels = [f"{q}q / {w}w" for w, q in zip(pivot_norm["Workers"], pivot_norm["Queries"])]
    x_indices = np.arange(len(q_vals))

    # --- LEFT PLOT: Execution Time ---
    for algo, color, marker in [("FCDS", "#1f77b4", "o"), ("Concurrent", "#d62728", "s")]:
        subset = summary[summary["Algo"] == algo].sort_values("Queries")
        if subset.empty: continue
        ax1.fill_between(subset["Queries"], subset["AvgTime"] - subset["StdTime"], 
                         subset["AvgTime"] + subset["StdTime"], color=color, alpha=0.15)
        ax1.plot(subset["Queries"], subset["AvgTime"], color=color, marker=marker, 
                 linewidth=2, label=algo, markeredgecolor='white')

    ax1.set_title("Total Execution Time", fontsize=14, fontweight='bold')
    ax1.set_ylabel("Time (ms)", fontsize=12)
    ax1.set_xticks(q_vals)
    ax1.set_xticklabels(x_labels)
    ax1.grid(True, ls="--", alpha=0.6)
    ax1.legend()

    # --- RIGHT PLOT: Normalized Insertions (Insertions / ms) ---
    bar_width = 0.35
    fcds_vals = pivot_norm["FCDS"] if "FCDS" in pivot_norm.columns else np.zeros(len(pivot_norm))
    conc_vals = pivot_norm["Concurrent"] if "Concurrent" in pivot_norm.columns else np.zeros(len(pivot_norm))

    ax2.bar(x_indices - bar_width/2, fcds_vals, bar_width, label='FCDS', color="#1f77b4", alpha=0.8)
    ax2.bar(x_indices + bar_width/2, conc_vals, bar_width, label='Concurrent', color="#d62728", alpha=0.8)

    ax2.set_title("Insertion Throughput (Normalized)", fontsize=14, fontweight='bold')
    ax2.set_ylabel("Insertions per ms", fontsize=12)
    ax2.set_xticks(x_indices)
    ax2.set_xticklabels(x_labels)
    ax2.grid(True, axis='y', ls="--", alpha=0.6)
    ax2.legend()

    plt.tight_layout()
    plt.savefig("plot_fixed_query_normalized_performance.png")
    print("Saved combined plot: plot_qr_normalized_performance.png")
    plt.show()