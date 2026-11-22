import sqlite3
from pathlib import Path


class ConversationStore:
    def __init__(self, path: str):
        self.path = path
        if path != ":memory:":
            Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self._connect() as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.execute("""CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY, conversation_id TEXT NOT NULL,
                role TEXT NOT NULL CHECK(role IN ('user','assistant')),
                content TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
            )""")
            db.execute("CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id)")

    def _connect(self) -> sqlite3.Connection:
        db = sqlite3.connect(self.path, timeout=10)
        db.row_factory = sqlite3.Row
        return db

    def add(self, conversation_id: str, role: str, content: str) -> None:
        with self._connect() as db:
            db.execute(
                "INSERT INTO messages(conversation_id,role,content) VALUES(?,?,?)",
                (conversation_id, role, content),
            )

    def history(self, conversation_id: str, limit: int = 20) -> list[dict[str, str]]:
        with self._connect() as db:
            rows = db.execute(
                "SELECT role,content FROM (SELECT id,role,content FROM messages WHERE conversation_id=? "
                "ORDER BY id DESC LIMIT ?) ORDER BY id ASC",
                (conversation_id, limit),
            ).fetchall()
        return [{"role": row["role"], "content": row["content"]} for row in rows]
