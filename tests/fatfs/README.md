# Real FatFS correctness tests

Run from the repository root:

```sh
python3 tests/fatfs/run.py
GBAREADER_FATFS_FIXTURE=legacy-v047.epub python3 tests/fatfs/run.py
```

Dependencies: Python 3, GCC/G++, `dosfstools` (`mkfs.fat`, `fsck.fat`) and `mtools` (`mcopy`). On Debian/Ubuntu, install the image tools with `apt-get install dosfstools mtools`.

The runner snapshots the repository's production storage/parser sources in a new temporary directory, records their SHA-256 hashes, and compiles the `__DEVKITARM__` ReaderFile branch with the actual FatFS implementation. Hardware initialization is discarded by linker section garbage collection; only an unused Butano header is stubbed. No production storage behavior is replaced by a mock.

A sector-backed FAT16 image exercises cache creation, subsequent bookmark saves and embedded TXT footers. Each verification uses a fresh process/mount. The tests check application reopening, normalized text, bookmark/settings/history, exact original EPUB entry data, standard ZIP CRCs, and read-only filesystem consistency.

## Pass/fail policy

Successful transactions and exercised one-shot disk read/write/sync failures are regression gates. A failure in these checks produces a nonzero exit code. Failure selectors that were not reached are listed separately and are not counted as exercised fault checks.

Persistent device failures and abrupt-process termination are diagnostic cases, not claims of guaranteed recovery. Existing TXT footer replacement can lose bookmark metadata if storage remains unavailable or execution stops during replacement, although the tested original text body remains intact. This release does not redesign that persistence format or add a recovery journal.

This harness does **not** test physical Supercard hardware, electrical power loss, torn sectors, card-internal caches, all possible failure positions, or out-of-space handling. It performs no speed benchmarks.

The runner prints its temporary evidence directory. Logs, reports, source hashes, extracted files and FAT images are retained there for investigation; no artifacts are written into the repository. Each run uses fresh images rather than mixing historical failures with current results.
