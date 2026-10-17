"""Build one source archive runnable with Python on Windows or Linux."""
import argparse
from pathlib import Path
import shutil
import tempfile
import zipapp


def build(output):
    root = Path(__file__).resolve().parent
    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="aco-host-build-") as directory:
        stage = Path(directory)
        shutil.copytree(root / "aco_host", stage / "aco_host",
                        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
        (stage / "__main__.py").write_text(
            "from aco_host.__main__ import main\nraise SystemExit(main())\n", encoding="utf-8")
        zipapp.create_archive(stage, output, interpreter="/usr/bin/env python3", compressed=True)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default=str(Path(__file__).resolve().parent / "build/aco-host.pyz"))
    print(build(parser.parse_args().output))
