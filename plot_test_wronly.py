#!/usr/bin/env python3
import os
import re
import pandas as pd
import matplotlib.pyplot as plt

# Path to your results directory
RESULTS_DIR = "build/test_wronly/"

def parse_files(directory):
    data = []
    # Regex to match the total program elapsed time
    total_time_re = re.compile(r"Total program elapsed time:\s+([\d.]+)\s+ms")
    
    if not os.path.exists(directory):
        print(f"Error: Directory {directory} not found.")
        return pd.DataFrame()

    for filename in os.listdir(directory):
        if not filename.endswith(".txt"):
            continue
            
        algo = "FCDS" if filename.startswith("fcds") else "Concurrent"
        threads_match = re.search(r"threads(\d+)", filename)
        
        if not threads_match:
            continue
            
        threads = int(threads_match.group(1))
        
        with open(os.path.join(directory, filename), 'r') as f:
            content = f.read()
            total_time_match = total_time_re.search(content)
            
            if total_time_match:
                data.append({
                    "Algo": algo,
                    "Threads": threads,
                    "TotalTime": float(total_time_match.group(1))
                })
                
    return pd.DataFrame(data)

# Load and process data
df = parse_files(RESULTS_DIR)

if df.empty:
    print("No data found. Check your RESULTS_DIR path and file contents.")
else:
    # AGGREGATION CHANGE: 
    # Calculate mean and std deviation of the TotalTime across the 10 runs
    summary = df.groupby(["Algo", "Threads"]).agg({
        "TotalTime": ["mean", "std"]
    }).reset_index()
    
    # Flatten multi-index columns
    summary.columns = ["Algo", "Threads", "AvgTime", "StdTime"]
    
    # Handle cases where std might be NaN (e.g., only 1 run)
    summary["StdTime"] = summary["StdTime"].fillna(0)

    # Aesthetic Plotting
    plt.style.use('seaborn-v0_8-muted')
    plt.figure(figsize=(10, 6), dpi=120)
    
    configs = [
        ("FCDS", "#1f77b4", "o"),      # Blue
        ("Concurrent", "#d62728", "s")   # Red
    ]
    
    for algo, color, marker in configs:
        subset = summary[summary["Algo"] == algo].sort_values("Threads")
        if subset.empty: continue
        
        # 1. Draw the shaded area (Standard Deviation across runs)
        # This will now be a narrow "ribbon" showing benchmark stability
        plt.fill_between(subset["Threads"], 
                         subset["AvgTime"] - subset["StdTime"], 
                         subset["AvgTime"] + subset["StdTime"], 
                         color=color, alpha=0.15, label='_nolegend_')
        
        # 2. Draw the main average line
        plt.plot(subset["Threads"], subset["AvgTime"], 
                 color=color, marker=marker, markersize=8, 
                 linewidth=2, label=algo, markeredgecolor='white')

    # Formatting
    plt.title("Writer-Only Performance", fontsize=14, fontweight='bold', pad=15)
    plt.xlabel("Number of Threads (Log Scale)", fontsize=12)
    plt.ylabel("Total Execution Time (ms)", fontsize=12)
    
    plt.xscale('log', base=2)
    all_threads = sorted(summary["Threads"].unique())
    plt.xticks(all_threads, all_threads)
    
    plt.grid(True, which="both", ls="--", alpha=0.6)
    plt.legend(frameon=True, facecolor='white', framealpha=1)
    
    plt.tight_layout()
    filename = "plot_wronly.png"
    plt.savefig(filename)
    print(f"Saved plot: {filename}")
    plt.show()