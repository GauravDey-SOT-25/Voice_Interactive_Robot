from backend.conversations import ConversationStore


def test_history_is_scoped_and_keeps_latest_messages(tmp_path):
    store = ConversationStore(str(tmp_path / "messages.sqlite3"))
    store.add("a", "user", "one")
    store.add("a", "assistant", "two")
    store.add("b", "user", "other")
    assert store.history("a") == [
        {"role": "user", "content": "one"},
        {"role": "assistant", "content": "two"},
    ]
    assert store.history("a", limit=1) == [{"role": "assistant", "content": "two"}]
