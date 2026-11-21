"""Inspect our own locally built Windows DLL: no execution or foreign files."""
import json
import subprocess
import sys


def main(parser, library):
    run = subprocess.run([parser, library], capture_output=True, text=True, check=True)
    info = json.loads(run.stdout)
    exports = {x["name"]: x for x in info["exports"]}
    assert exports.get("ScarlettAdd") == {
        "name": "ScarlettAdd", "ordinal": 1, "forwarder": None}, exports
    assert exports.get("ScarlettMultiply") == {
        "name": "ScarlettMultiply", "ordinal": 7, "forwarder": None}, exports
    assert info["executable_tested"] is False, info
    print("Native source-built DLL PE32+ export inventory PASS")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
