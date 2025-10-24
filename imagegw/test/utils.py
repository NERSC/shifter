import os
import pytest


@pytest.fixture
def set_path(monkeypatch):
    test_dir = os.path.dirname(os.path.abspath(__file__))
    monkeypatch.setenv("PATH", f"{os.environ['PATH']}:{test_dir}/fakebin")
