from pydantic import BaseModel
from enum import Enum, auto


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


class Operations(Enum):
    PULL = auto()
    IMPORT = auto()


class Request(BaseModel):
    op: Operations
    system: str
    itype: str
    tag: str
    format: str = "squashfs"
    userACL: list[int] | None = []
    groupACL: list[int] | None = []

    def query_ready(self):
        return {
            'status': 'READY',
            'system': self.system,
            'itype': self.itype,
            'tag': {'$in': [self.tag]}
        }

    def query_by_pull(self):
        return {
            'system': self.system,
            'itype': self.itype,
            'pulltag': self.tag
        }

    def pull_record(self, session: Session):

        def _make_acl(acllist: dict, id: str):
            if id not in acllist:
                acllist.append(id)
            return acllist

        rec = {
            'system': self.system,
            'itype': self.itype,
            'pulltag': self.tag
        }
        rec['userACL'] = []
        rec['groupACL'] = []
        if self.userACL and self.userACL != []:
            rec['userACL'] = _make_acl(self.userACL, session.uid)
        if self.groupACL and self.groupACL != []:
            rec['groupACL'] = _make_acl(self.groupACL, session.gid)
        return rec
