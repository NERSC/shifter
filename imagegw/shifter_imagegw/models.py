from pydantic import BaseModel


class Session(BaseModel):
    uid: int
    gid: int
    system: str
    user: str
    group: str
    tokens: str = ""

    def __hash__(self):
        return hash((self.uid, self.gid, self.system,
                    self.user, self.group, self.tokens))
