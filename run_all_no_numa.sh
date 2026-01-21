#!/bin/bash

# Loop through all .sh files in the current directory
for script in *.sh; do
    # 1. Check if the file exists
    # 2. Exclude "wronly"
    # 3. Exclude "numa"
    # 4. Exclude "compile"
    if [[ -f "$script" && ! "$script" =~ "wronly" && ! "$script" =~ "numa" && ! "$script" =~ "compile" ]]; then
        echo "================================================"
        echo "RUNNING TEST: $script"
        echo "================================================"
        
        # Ensure execution permissions
        chmod +x "$script"
        
        # Run the script and wait for it to finish
        ./"$script"
        
        echo -e "\nCompleted: $script\n"
    fi
done

echo "All specified tests finished."
