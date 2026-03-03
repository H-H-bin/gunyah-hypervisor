#!/usr/bin/env python3
"""
Script to extract CR numbers from git commit logs.

Usage:
  python3 extract_cr_numbers.py <branch-name>
  python3 extract_cr_numbers.py <base-branch> <feature-branch>

Single branch mode: Extracts all CR numbers from the specified branch.
Two branch mode: Finds non-merged commits in feature-branch that don't exist in
base-branch, extracts CR numbers from those commits, and reports which CRs also
exist in base-branch.
"""

import sys
import subprocess
import re
from typing import List


def run_git_log(branch_name: str) -> str:
    """Run git log command and return the output."""
    try:
        cmd = ['git', 'log', '-s', '--reverse', '--pretty=full', branch_name]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error running git command: {e}", file=sys.stderr)
        print(f"Git stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)
    except FileNotFoundError:
        print("Error: git command not found. "
              "Make sure git is installed and in PATH.", file=sys.stderr)
        sys.exit(1)


def get_non_merged_commits(base_branch: str, feature_branch: str) -> str:
    """Get commits that exist in feature_branch but not in base_branch."""
    try:
        # Use git log to find commits in feature_branch that are not in
        # base_branch
        cmd = ['git', 'log', '-s', '--reverse',
               '--pretty=full', f'{base_branch}..{feature_branch}']
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error running git command to find non-merged commits: {e}",
              file=sys.stderr)
        print(f"Git stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)


def extract_cr_numbers(git_log_output: str) -> List[str]:
    """Extract CR numbers from git log output, preserving order and removing
       duplicates."""
    # Pattern to match "Fixes: CR-nnnnn" where nnnnn is one or more digits
    cr_pattern = re.compile(r'Fixes:\s+CR-(\d+)', re.IGNORECASE)

    cr_numbers = []
    seen_crs = set()

    for line in git_log_output.split('\n'):
        line = line.strip()
        matches = cr_pattern.findall(line)

        for cr_number in matches:
            cr_tag = f"CR-{cr_number}"
            if cr_tag not in seen_crs:
                cr_numbers.append(cr_tag)
                seen_crs.add(cr_tag)

    return cr_numbers


def main():
    """Main function."""
    if len(sys.argv) == 2:
        # Single branch mode
        branch_name = sys.argv[1]

        # Run git log command
        git_output = run_git_log(branch_name)

        # Extract CR numbers
        cr_numbers = extract_cr_numbers(git_output)

        # Print results
        if cr_numbers:
            for cr in cr_numbers:
                print(cr)
        else:
            print("No CR numbers found in the git log.", file=sys.stderr)

    elif len(sys.argv) == 3:
        # Two branch comparison mode
        base_branch = sys.argv[1]
        feature_branch = sys.argv[2]

        print(f"Comparing branches: {base_branch} (base) vs {feature_branch} "
              "(feature)")
        print("=" * 60)

        # Get CR numbers from base branch
        base_git_output = run_git_log(base_branch)
        base_cr_numbers = extract_cr_numbers(base_git_output)
        base_cr_set = set(base_cr_numbers)

        print(f"CR numbers in base branch ({base_branch}): "
              f"{len(base_cr_numbers)}")
        if base_cr_numbers:
            if len(base_cr_numbers) > 10:
                print("(last 10 CRs)")
            for cr in base_cr_numbers[-10:]:
                print(f"  {cr}")
        else:
            print("  None found")
        print()

        # Get non-merged commits from feature branch
        non_merged_output = get_non_merged_commits(base_branch, feature_branch)
        feature_cr_numbers = extract_cr_numbers(non_merged_output)

        print("CR numbers in non-merged commits of feature branch "
              f"({feature_branch}): {len(feature_cr_numbers)}")
        if feature_cr_numbers:
            for cr in feature_cr_numbers:
                print(f"  {cr}")
        else:
            print("  None found")
        print()

        # Find overlapping CR numbers
        overlapping_crs = []
        for cr in feature_cr_numbers:  # Preserve order from feature branch
            if cr in base_cr_set:
                overlapping_crs.append(cr)

        print("CR numbers from feature branch that also exist in base branch: "
              f"{len(overlapping_crs)}")
        if overlapping_crs:
            for cr in overlapping_crs:
                print(f"  {cr}")
            sys.exit(1)
        else:
            print("  None found")

    else:
        print("Usage:", file=sys.stderr)
        print("  python3 extract_cr_numbers.py <branch-name>", file=sys.stderr)
        print("  python3 extract_cr_numbers.py <base-branch> <feature-branch>",
              file=sys.stderr)
        print("", file=sys.stderr)
        print("Examples:", file=sys.stderr)
        print("  python3 extract_cr_numbers.py origin/main", file=sys.stderr)
        print("  python3 extract_cr_numbers.py origin/main feature-branch",
              file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
