---
name: Bug report
about: Report crashes or unexpected behavior
title: ''
labels: bug
assignees: ''

---

FIRST: Before reporting any bug, make sure that the bug you are reporting has not been reported before. Also, try to use the [nightly version](https://www.sdrpp.org/nightly) if possible in case I've already fixed the bug.

**Hardware**
- CPU: 
- GPU: 
- SDR: (Remote or local? If remote, what protocol?)

**Software**
- Operating System: Name + Exact version (eg. Windows 10 x64, Ubuntu 22.04, MacOS 10.15)
- SDR++: Version + Build date (available either in the window title or in the credits menu which you can access by clicking on the SDR++ icon in the top right corner of the software).

**Bug Description**
A clear description of the bug.

**Steps To Reproduce**
1. ...
2. ...
3. ...

**Only If SDR++ fails to lauch or the SDR fails to start:**
Run SDR++ from a command line window with special parameters:
* On Windows, open a terminal and `cd` to SDR++'s directory and run `.\sdrpp.exe -c` (if running SDR++ version 1.0.4 or older, use `-s` instead, though you should probably update SDR++ instead...)
* On Linux: Open a terminal and run `sdrpp -c`
* On MacOS: Open a terminal and run `/path/to/the/SDR++.app/Contents/MacOS/sdrpp -c`
Then, post the **entire** logs from start to after the issue. **DOT NOT truncate to where you *think* the error is...**

**Screenshots**
Add any screenshot that is relevant to the bug (GUI error messages, strange behavior, graphics glitch, etc...).

**Additional info**
Add any other relevant information.
