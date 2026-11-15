# Repository setup

The one-time steps an org owner runs after creating a repository from the template, and before the first PR.

The scripts are in [`kokeb-labs/.github/org/`](https://github.com/kokeb-labs/.github/tree/main/org). Each one is a dry run
unless given `--apply`; `--org` names the organisation (a driver lives in `kokeb-labs-drivers`, everything else in
`kokeb-labs`) and `--repo` limits it to one repository. The rules behind them are in
[repo-governance.md](https://github.com/kokeb-labs/.github/blob/main/standards/repo-governance.md).

## 1. Create and scaffold

```bash
gh repo create kokeb-labs-drivers/driver-<chip> --private --template kokeb-labs/driver-template --clone
cd driver-<chip>
git switch -c feat/initial-driver
python3 tools/setup.py
```

`tools/setup.py` asks for the bus and a one-sentence description, scaffolds, builds and runs the host tests;
`tools/scaffold.py --help` lists every option (`--platforms host,esp32` is a driver's default; a library gets
`host` unless `esp32` is asked for); it ends with a checklist of what is left to fill in.

## 2. Apply the org settings

From a checkout of `kokeb-labs/.github`, after adding the repository to `org/repos.json` by PR. Its entry sets
the About: the description is the README's first sentence (`C++20 driver for the <vendor> <parts> <what it is>
(<bus>)`), and the topics are the shared `kokeb-labs driver embedded cpp20 misra-cpp header-only` plus the bus, the
device category, the vendor and the part (`<bus> <category> <vendor> <part>`):

```bash
python3 org/kokeb_org.py --org kokeb-labs-drivers --repo driver-<chip>           # the plan
python3 org/kokeb_org.py --org kokeb-labs-drivers --repo driver-<chip> --apply   # settings, Actions, labels, ...
```

A private repository on the free plan gets the settings, Actions policy and labels; security features and
rulesets need it public (the tool says which sections it skips). Run it again, with `--check` to see that nothing is
pending, after the repository goes public (section 6).

Run it without `--apply` first to see what changes.

## 3. Create the labels

`--apply` in step 2 already creates them; this is the manual way, for a repository outside the org tool.

```bash
python3 - <<'PY'
import subprocess, yaml
for label in yaml.safe_load(open(".github/labels.yml")):
    subprocess.run(["gh", "label", "create", label["name"], "--color", label["color"],
                    "--description", label["description"], "--force"], check=True)
PY
```

## 4. Require the gate

After the first CI run has produced the check `CI gate / gate`, add it to the repository's `main-review` ruleset as
described in [The required check](https://github.com/kokeb-labs/.github/blob/main/standards/repo-governance.md#the-required-check).

## 5. Wire it in

Add the repository as a submodule or CMake dependency where it is consumed, in its own pin PR, then
follow [checklists/new-driver.md](https://github.com/kokeb-labs/.github/blob/main/standards/checklists/new-driver.md).

## 6. Going public and the first release candidate

The order that works, and the reason for each step (the `publish-driver` skill has the commands and the checks):

1. **Iterate privately.** The driver is built, reviewed and rehearsed in a private repository or locally until the
   `first-push` skill passes. Keep a local copy (a git bundle and the tree) outside the repository.
2. **Publish a fresh copy.** An agent prepares one clean commit of the reviewed tree, with no history, and scans it
   (history, identity, secrets, PDFs, names). The owner makes the public repository, empty
   (`gh repo create <org>/<repo> --public`, or an emptied repository made public with
   `gh repo edit <org>/<repo> --visibility public --accept-visibility-change-consequences`), and the commit is pushed
   to `main` as a plain push. Replacing the history of an existing repository is the owner's, with the backup open.
3. **`main` green.** Wait for `CI` on `main`, including `CI gate / gate`. The docs publish to `gh-pages` from `main`
   before any tag does. Triage a failure with the `release` skill, section 5: re-run the failed jobs only when the
   infrastructure failed.
4. **Settings.** `python3 org/kokeb_org.py --org kokeb-labs-drivers --repo driver-<chip> --apply`: rulesets, secret
   scanning with push protection, CodeQL, private vulnerability reporting and the labels need a public repository.
   If the last call answers 422, run it again after a few seconds; the new visibility has not reached that endpoint
   yet. Add `CI gate / gate` to the `main-review` ruleset (section 4).
5. **Enable Pages** from the `gh-pages` branch, root, and open `https://<org>.github.io/<repo>/`.
6. **`--check`** reports `0 change(s) pending`.
7. **Tag `vX.Y.Z-rc.1`** on that commit (the `release` skill). Release candidates come after the repository is
   public, `main` is green and Pages is enabled. Its CI repeats the run, then the Release workflow publishes the
   pre-release: firmware, SBOM, checksums. A pre-release publishes its docs under its own tag and takes `latest` only
   when no other version holds it. The release text states the hardware level (`hardware-validation` skill). The final
   tag is the owner's.

A private repository on the free plan shares a small Actions storage quota with the rest of its organisation. A full
quota fails every upload step in every repository of it at once, which looks like a broken workflow and is not one;
a public repository does not count against it.
