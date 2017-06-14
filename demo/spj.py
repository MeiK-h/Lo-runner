import lorun

def special(spj_path, stdin_path, stdout_path, userout_path):
    spjcfg = {
        'args': [spj_path, stdin_path, stdout_path, userout_path],
        'timelimit': 1000, #in MS
        'memorylimit': 20000, #in KB
    }
    outbuffer = lorun.special(spjcfg)
    if outbuffer:
        print outbuffer
        return False
    print 'AC'
    return True

if "__main__" == __name__:
    import sys
    if len(sys.argv) != 5:
        print 'Usage:%s spj_path, stdin_path, stdout_path, userout_path'
        exit(-1)
    special(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])