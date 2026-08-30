#!/usr/bin/env python3
import os
import re
import sys

LOG_DIR = "bench_narrow_logs"

def main():
    if not os.path.exists(LOG_DIR):
        print(f"Error: Directory '{LOG_DIR}' not found.")
        sys.exit(1)

    # Grab .log files and enforce specific order: Spheres, Mixed, Boxes
    files = [f for f in os.listdir(LOG_DIR) if f.endswith('.log')]

    def sort_key(filename):
        if "100_spheres" in filename: return 1
        if "mixed_equal" in filename: return 2
        if "100_boxes" in filename: return 3
        return 4 # Puts any other configurations (like 98_boxes) at the end

    files.sort(key=sort_key)

    if not files:
        print(f"No log files found in '{LOG_DIR}'.")
        sys.exit(1)

    # Track algorithms in order
    algos = ["simple_naive", "execute_indirect", "work_graphs"]

    # Store data: data[algo][scenario_index (1,2,3)][phase] = float
    data = {algo: {i: {} for i in range(1, len(files) + 1)} for algo in algos}

    # Parse files
    for idx, filename in enumerate(files, 1):
        filepath = os.path.join(LOG_DIR, filename)

        with open(filepath, 'r') as f:
            lines = f.readlines()

        current_algo = None
        for line in lines:
            if "simple_naive" in line:
                current_algo = "simple_naive"
            elif "execute_indirect" in line:
                current_algo = "execute_indirect"
            elif "work_graphs" in line:
                current_algo = "work_graphs"
            elif "GPU:" in line and current_algo:
                # Regex extracts tuples like: ('build', '0.502')
                matches = re.findall(r'(\w+)=([\d\.]+)ms', line)
                for phase, val in matches:
                    if phase in ["build", "query"]:
                        # Merge build and query into a single 'broad' phase
                        data[current_algo][idx]["broad"] = data[current_algo][idx].get("broad", 0.0) + float(val)
                    else:
                        data[current_algo][idx][phase] = float(val)
                current_algo = None # reset for next line

    # Metadata for styling
    algo_meta = {
        "simple_naive": {"name": "Baseline", "color": "teal", "shift": "-18pt"},
        "execute_indirect": {"name": "Execute Indirect", "color": "orange", "shift": "0pt"},
        "work_graphs": {"name": "Work Graphs", "color": "violet!80!black", "shift": "18pt"}
    }

    # Color intensity approach
    # gap_narrow is explicitly white as requested.
    # Phase definition: Merged broad phase, empty gap_narrow, solid narrow.
    phases = ["broad", "gap_narrow", "narrow"]
    phase_meta = {
        "broad": "fill={color}!50, draw={color}",
        "gap_narrow": "fill=white, draw={color}",
        "narrow": "fill={color}, draw={color}"
    }

    # Generate LaTeX output
    for i, algo in enumerate(algos):
        print(f"    % =================================")
        print(f"    % {algo_meta[algo]['name']}")
        print(f"    % =================================")

        # Determine if we should append 'forget plot'
        forget_str = ", forget plot" if algo != "simple_naive" else ""
        color = algo_meta[algo]['color']
        shift = algo_meta[algo]['shift']

        for phase in phases:
            # Check if this phase was recorded for this algorithm in any scenario
            has_phase = any(phase in data[algo][s_idx] for s_idx in data[algo])
            if not has_phase:
                continue

            print(f"    % {phase.replace('_', ' ').title()} Phase")
            style = phase_meta[phase].format(color=color)

            print(f"    \\addplot+[{style}, bar shift={shift}{forget_str}] coordinates {{")

            # Print coordinates on a single line
            coords = []
            for idx in sorted(data[algo].keys()):
                val = data[algo][idx].get(phase, 0.0)
                coords.append(f"({idx}, {val:.3f})")
            print("        " + " ".join(coords))
            print("    };")

        # Total times (invisible bar just for the labels)
        print("    % Total Times")
        node_style = "every node near coord/.append style={anchor=south, font=\\scriptsize, color=black}"
        print(f"    \\addplot+[bar shift={shift}{forget_str}, point meta=explicit, nodes near coords, {node_style} ] coordinates {{")

        coords = []
        for idx in sorted(data[algo].keys()):
            val = data[algo][idx].get("total", 0.0)
            coords.append(f"({idx}, 0.001) [{val:.3f}]")
        print("        " + " ".join(coords))
        print("    };")

        # Inject stack reset between algorithms
        if i < len(algos) - 1:
            print("\n    % Stack Reset")
            print("    \\makeatletter")
            print("    \\pgfplots@stacked@isfirstplottrue")
            print("    \\makeatother\n")

if __name__ == "__main__":
    main()
