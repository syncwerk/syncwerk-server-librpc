import json

from common import RpcsyncwerkError

class RpcsyncwerkService(object):
    def __init__(self, name):
        self.name = name
        self.func_table = {}

class RpcsyncwerkServer(object):
    def __init__(self):
        self.services = {}

    def create_service(self, svcname):
        service = RpcsyncwerkService(svcname)
        self.services[svcname] = service

    def register_function(self, svcname, fn, fname=None):
        service = self.services[svcname]
        if fname == None:
            fname = fn.__name__
        service.func_table[fname] = fn

    def _call_function(self, svcname, fcallstr):
        """input str -> output str"""
        try:
            argv = json.loads(fcallstr)
        except Exception, e:
            raise RpcsyncwerkError('bad call str: ' + str(e))

        service = self.services[svcname]

        fname = argv[0]
        fn = service.func_table.get(fname, None)
        if fn is None:
            raise RpcsyncwerkError('No such funtion %s' % fname)

        ret = fn(*argv[1:])
        return ret

    def call_function(self, svcname, fcallstr):
        try:
            retVal = self._call_function(svcname, fcallstr)
        except Exception, e:
            ret = {'err_code': 555, 'err_msg': str(e)}
        else:
            ret = {'ret': retVal}

        return json.dumps(ret)

rpcsyncwerk_server = RpcsyncwerkServer()
