# Real FatFS correctness tests

Run from the repository root:

```sh
python3 tests/fatfs/run.py
GBAREADER_FATFS_FIXTURE=legacy-v047.epub python3 tests/fatfs/run.py
```

Dependencies: Python 3, GCC/G++, `dosfstools` (`mkfs.fat`, `fsck.fat`) and `mtools` (`mcopy`). On Debian/Ubuntu, install the image tools with `apt-get install dosfstools mtools`.

The runner snapshots the repository's production storage/parser sources in a new temporary directory, records their SHA-256 hashes, and compiles the `__DEVKITARM__` ReaderFile branch with the actual FatFS implementation. Hardware initialization is discarded by linker section garbage collection; only an unused Butano header is stubbed. No production storage behavior is replaced by a mock.

A sector-backed FAT16 image exercises cache creation, subsequent bookmark saves and embedded TXT footers. Verification uses fresh processes/mounts and explicit `live-retry` / `live-retry-nav` actions on the same ReaderFile/EpubDocument after one-shot source read errors, for TXT and EPUB with and without prior state. The tests check application reopening, normalized text, bookmark/settings/history, exact source bytes, standard ZIP CRCs, and read-only filesystem consistency.

Unknown empty and nonempty SAV fixtures must refuse mutation and suppress embedded state. Real partial-header crash records are seeded separately with checked matching headers, not inferred from returned-error cleanup (which can truncate to zero). The opening-mode failure test verifies empty-artifact refusal after reopening, then explicitly backs up and moves aside the fixture using computer-side `mcopy`/`mren` before a successful fresh save. No production automatic deletion/adoption is permitted.

## Pass/fail policy

Successful transactions and exercised one-shot disk read/write/sync failures are regression gates. A failure in these checks produces a nonzero exit code. Failure selectors that were not reached are listed separately and are not counted as exercised fault checks.

Persistent device failures and abrupt-process termination are diagnostic cases, not claims of guaranteed recovery. Sources remain read-only; failed initial SAV creation may leave no recoverable state, and an empty artifact requires manual backup/move-aside after restart. Updates to existing valid companions must retain complete old-or-new checked state in the exercised cases.

This harness does **not** test physical Supercard hardware, electrical power loss, torn sectors, card-internal caches, all possible failure positions, or out-of-space handling. It performs no speed benchmarks.

The runner prints its temporary evidence directory. Logs, reports, source hashes, extracted files and FAT images are retained there for investigation; no artifacts are written into the repository. Each run uses fresh images rather than mixing historical failures with current results.
