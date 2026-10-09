# Contributing

- Build with `make`, run the checks with `make test`.
- Keep the build warning-clean. CI builds with `WERROR=1`, so warnings fail the
  build before they reach review.
- Match the code already in `include/` and `src/`: C11, Allman braces, one idea
  per file, and no comment that only repeats what the line already says.
- Keep the public surface in `include/` small and documented, because that is
  what a client is allowed to depend on.
- Add a test with a change when the change is something a test can hold.
- Send a pull request with one focused change and a short note on why.
