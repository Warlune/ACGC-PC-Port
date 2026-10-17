# World gate validation

The mortgage gates were exercised with the isolated development executable and
separate copies of the converted 31-by-18 three-town save. Each process used
the normal hidden CLI test window and `--verbose` so its assertions were
captured in the test log. No test ran against the active save.

Commands used:

```text
AnimalCrossing.exe --verbose --dev-world --dev-progress-stage 0 --dev-coast-test --dev-se
AnimalCrossing.exe --verbose --dev-world --dev-progress-stage 1 --dev-coast-test --dev-sw
AnimalCrossing.exe --verbose --dev-world --dev-progress-stage 2 --dev-coast-test --dev-sw
AnimalCrossing.exe --verbose --dev-world --dev-progress-stage 2 --dev-coast-test --dev-se
```

The stage 0 test approached the closed eastern gate at seam 19 from the native
`--dev-se` spawn (x=11420). It stopped at x=12142, before the seam at x=12160,
then retreated successfully. The stage 1 test approached the closed western
gate at seam 12 from the native `--dev-sw` spawn (x=8420). It stopped at x=7698,
before the seam at x=7680, then retreated successfully. Both tests required at
least 200 units of approach and 40 units of retreat, and verified the player
stayed in the outdoor scene.

At stage 2, both the SW and SE coast tests crossed into the neighboring town
and back, then pushed against each outer corner and escaped. Both finished with
`PASS: ... crossing both ways, both corner pushes and escapes`. All runtime
test stderr logs were empty.

The minimap atlas signatures were `b25204f3` at stage 0, `b4520819` at stage 1,
and `b3520686` at stage 2. Their change with the unlock mask confirms the
cached terrain atlas is rebuilt for each gate state. The progression smoke test
also passed, including partial and full repayment transitions, zero initial
debt, multi-resident unlocks, high-tier saves, preservation of unrelated home
pad bits, and keeping sea/water collision attributes intact.

The stage override is process-local and does not mutate mortgage progress in
the save. Runtime tests therefore validate closed/open collision and coast
traversal at each state; they do not simulate making a payment through the live
post-office UI or verify a same-session atlas recapture after an actual payoff.
