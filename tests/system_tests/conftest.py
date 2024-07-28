import pathlib  # noqa: D100


def pytest_addoption(parser):  # noqa: D103
    parser.addoption(
        "--j4dd-executable", action="store", type=pathlib.Path, required=True
    )
