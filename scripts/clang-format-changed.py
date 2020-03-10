#!/usr/bin/env python

import argparse
import os
import sys
import subprocess


def check_file(filename, excludes, extensions):
    """
    Check if a file should be included in our check
    """
    name, ext = os.path.splitext(filename)

    if len(ext) > 0 and ext in extensions:
        if len(excludes) == 0:
            return True

        for exclude in excludes:
            if exclude in filename:
                return False

        return True

    return False


def check_directory(directory, excludes, extensions):
    output = []

    if len(excludes) > 0:
        for exclude in excludes:
            if exclude in directory:
                directory_excluded = False
                return output

    for root, _, files in os.walk(directory):
        for file in files:
            filename = os.path.join(root, file)
            if check_file(filename, excludes, extensions):
                # print("Will check file [{}]".format(filename))
                output.append(filename)
    return output

def get_git_root(git_bin):
    cmd = [git_bin, "rev-parse", "--show-toplevel"]
    try:
        return subprocess.check_output(cmd).strip()
    except subprocess.CalledProcessError, e:
        print("Error calling git [{}]".format(e))
        raise

def clean_git_filename(line):
    """
    Takes a line from git status --porcelain and returns the filename
    """
    file = None
    git_status = line[:2]
    # Not an exhaustive list of git status output but should
    # be enough for this case
    # check if this is a delete
    if 'D' in git_status:
        return None
    # ignored file
    if '!' in git_status:
        return None
    # Covers renamed files
    if '->' in line:
        file = line[3:].split('->')[-1].strip()
    else:
        file = line[3:].strip()

    return file


def get_changed_files(git_bin, excludes, file_extensions):
    """
    Run git status and return the list of changed files
    """
    extensions = file_extensions.split(",")
    # arguments coming from cmake will be *.xx. We want to remove the *
    for i, extension in enumerate(extensions):
        if extension[0] == '*':
            extensions[i] = extension[1:]

    git_root = get_git_root(git_bin)

    cmd = [git_bin, "status", "--porcelain", "--ignore-submodules"]
    # print("git cmd = {}".format(cmd))
    output = []
    returncode = 0
    try:
        cmd_output = subprocess.check_output(cmd)
        for line in cmd_output.split('\n'):
            if len(line) > 0:
                file = clean_git_filename(line)
                if not file:
                    continue
                file = os.path.join(git_root, file)

                if file[-1] == "/":
                    directory_files = check_directory(
                        file, excludes, file_extensions)
                    output = output + directory_files
                else:
                    if check_file(file, excludes, file_extensions):
                        # print("Will check file [{}]".format(file))
                        output.append(file)

    except subprocess.CalledProcessError, e:
        print("Error calling git [{}]".format(e))
        returncode = e.returncode

    return output, returncode


def run_clang_format(clang_format_bin, changed_files, check_only):
    """
    Run clang format on a list of files 
    @return 0 if formatted correctly.
    """
    if len(changed_files) == 0:
        return 0

    if check_only:
        cmd = [clang_format_bin, "-style=file", "-output-replacements-xml"] + changed_files
        print("Will check format on changed files {}".format(changed_files))
    else:
        cmd = [clang_format_bin, "-style=file", "-i"] + changed_files
        print("Will format changed files {}".format(changed_files))
    # print("clang-format cmd = {}".format(cmd))
    try:
        cmd_output = subprocess.check_output(cmd)
        if check_only:
            if "replacement offset" in cmd_output:
                print("ERROR: Changed files {} don't match format".format(changed_files))
                return 1
            else:
                print("Format OK for changed files {}".format(changed_files))
    except subprocess.CalledProcessError, e:
        print("Error calling clang-format [{}]".format(e))
        return e.returncode

    return 0


def cli():
    # global params
    parser = argparse.ArgumentParser(prog='clang-format-check-changed',
                                     description='Checks if files chagned in git match the .clang-format specification')
    parser.add_argument("--file-extensions", type=str,
                        default=".c,.cpp,.h,.cxx,.hxx,.hpp,.cc,.ipp",
                        help="Comma separated list of file extensions to check")
    parser.add_argument('--exclude', action='append', default=[],
                        help='Will not match the files / directories with these in the name')
    parser.add_argument('--check-only', action='store_true', default=True,
                        help='Only perform format check but does not format')
    parser.add_argument('-i', '--in-place', action='store_true', 
                        help='Apply format in place')
    parser.add_argument('--clang-format-bin', type=str, default="clang-format",
                        help="The clang format binary")
    parser.add_argument('--git-bin', type=str, default="git",
                        help="The git binary")
    args = parser.parse_args()

    if args.in_place:
        # override the check_only
        args.check_only = False

    # Run gcovr to get the .gcda files form .gcno
    changed_files, returncode = get_changed_files(
        args.git_bin, args.exclude, args.file_extensions)
    if returncode != 0:
        return returncode

    return run_clang_format(args.clang_format_bin, changed_files, args.check_only)

if __name__ == '__main__':
    sys.exit(cli())
