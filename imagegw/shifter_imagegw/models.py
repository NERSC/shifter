from pydantic import BaseModel
from enum import Enum, auto
from typing import Any


class Session(BaseModel):
    uid: int
    gid: int
    system: str
    user: str
    group: str
    tokens: str = ""
    admin: bool = False

    def __hash__(self):
        return hash((self.uid, self.gid, self.system,
                    self.user, self.group, self.tokens))


class Operations(Enum):
    PULL = auto()
    IMPORT = auto()


class Request(BaseModel):
    op: Operations
    session: Session
    itype: str
    tag: str
    format: str = "squashfs"
    filepath: str | None = None
    userACL: list[int] | None = []
    groupACL: list[int] | None = []
    system: str | None = None

    def model_post_init(self, context: Any) -> None:
        def _make_acl(acllist: dict, id: str):
            if id not in acllist:
                acllist.append(id)
            return acllist
        self.system = self.session.system
        if self.userACL and self.userACL != []:
            self.userACL = _make_acl(self.userACL, self.session.uid)
        if self.groupACL and self.groupACL != []:
            self.groupACL = _make_acl(self.groupACL, self.session.gid)

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
