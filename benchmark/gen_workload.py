import sys
import os

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

#####################################################################################

def reverseHostName ( email ) :
    name, sep, host = email.partition('@')
    hostparts = host[:-1].split('.')
    r_host = ''
    for part in hostparts :
        r_host = part + '.' + r_host
    return r_host + sep + name

#####################################################################################

if (len(sys.argv) < 3) :
    print bcolors.FAIL + 'Usage:'
    print 'arg 1, workload type: workloadc, workloade'
    print 'arg 2, key type: randint, email'
    print 'optional arg 3, email list'
    print 'optional arg 4, number of email records' + bcolors.ENDC
    sys.exit()

workload = sys.argv[1]
key_type = sys.argv[2]

print bcolors.OKGREEN + 'workload type = ' + workload
print 'key type = ' + key_type + bcolors.ENDC

ycsb_dir = 'YCSB/bin/'
workload_dir = 'workload_spec/'
output_dir='workloads/'

email_list = ''
email_list_size = 0

if (len(sys.argv) > 4) :
    email_list = sys.argv[3]
    email_list_size = int(sys.argv[4])

'''
email_list = 'list.txt'
email_list_size = 27549660
'''

if workload != 'workloadc' and workload != 'workloade' :
    print bcolors.FAIL + 'Incorrect workload: please pick from workloadc and workloade' + bcolors.ENDC
    sys.exit()

if key_type != 'randint' and key_type != 'email' :
    print bcolors.FAIL + 'Incorrect key_type: please pick from randint and email' + bcolors.ENDC
    sys.exit()

if key_type == 'email' and email_list_size == 0 :
    print bcolors.FAIL + 'please provide your email list and the total number of email records' + bcolors.ENDC
    sys.exit()

out_ycsb_load = output_dir + 'ycsb_load_' + key_type + '_' + workload
out_ycsb_txn = output_dir + 'ycsb_txn_' + key_type + '_' + workload
out_load_ycsbkey = output_dir + 'load_' + 'ycsbkey' + '_' + workload
out_txn_ycsbkey = output_dir + 'txn_' + 'ycsbkey' + '_' + workload
out_load = output_dir + 'load_' + key_type + '_' + workload
out_txn = output_dir + 'txn_' + key_type + '_' + workload

cmd_ycsb_load = ycsb_dir + 'ycsb load basic -P ' + workload_dir + workload + ' -s > ' + out_ycsb_load
cmd_ycsb_txn = ycsb_dir + 'ycsb run basic -P ' + workload_dir + workload + ' -s > ' + out_ycsb_txn

os.system(cmd_ycsb_load)
os.system(cmd_ycsb_txn)

#####################################################################################

f_load = open (out_ycsb_load, 'r')
f_load_out = open (out_load_ycsbkey, 'w')
for line in f_load :
    cols = line.split()
    if len(cols) > 0 and cols[0] == "INSERT":
        f_load_out.write (cols[0] + " " + cols[2][4:] + "\n")
f_load.close()
f_load_out.close()

f_txn = open (out_ycsb_txn, 'r')
f_txn_out = open (out_txn_ycsbkey, 'w')
for line in f_txn :
    cols = line.split()
    if (cols[0] == 'SCAN') or (cols[0] == 'INSERT') or (cols[0] == 'READ') or (cols[0] == 'UPDATE'):
        startkey = cols[2][4:]
        if cols[0] == 'SCAN' :
            numkeys = cols[3]
            f_txn_out.write (cols[0] + ' ' + startkey + ' ' + numkeys + '\n')
        else :
            f_txn_out.write (cols[0] + ' ' + startkey + '\n')
f_txn.close()
f_txn_out.close()

cmd = 'rm -f ' + out_ycsb_load
os.system(cmd)
cmd = 'rm -f ' + out_ycsb_txn
os.system(cmd)

#####################################################################################

if key_type == 'randint' :
    f_load = open (out_load_ycsbkey, 'r')
    f_load_out = open (out_load, 'w')
    for line in f_load :
        f_load_out.write (line)

    f_txn = open (out_txn_ycsbkey, 'r')
    f_txn_out = open (out_txn, 'w')
    for line in f_txn :
        f_txn_out.write (line)

elif key_type == 'email' :
    if email_list_size == 0 :
        print bcolors.FAIL + 'please provide your email list and the total number of email records' + bcolors.ENDC
        sys.exit()

    keymap = {}
    f_email = open (email_list, 'r')
    emails = f_email.readlines()

    f_load = open (out_load_ycsbkey, 'r')
    f_load_out = open (out_load, 'w')

    sample_size = len(f_load.readlines())
    gap = email_list_size / sample_size

    f_load.close()
    f_load = open (out_load_ycsbkey, 'r')
    count = 0
    for line in f_load :
        cols = line.split()
        email = reverseHostName(emails[count * gap])
        keymap[int(cols[1])] = email
        f_load_out.write (cols[0] + ' ' + email + '\n')
        count += 1

    count = 0
    f_txn = open (out_txn_ycsbkey, 'r')
    f_txn_out = open (out_txn, 'w')
    for line in f_txn :
        cols = line.split()
        if cols[0] == 'SCAN' :
            f_txn_out.write (cols[0] + ' ' + keymap[int(cols[1])] + ' ' + cols[2] + '\n')
        elif cols[0] == 'INSERT' :
            email = reverseHostName(emails[count * gap + 1])
            keymap[int(cols[1])] = email
            f_txn_out.write (cols[0] + ' ' + email + '\n')
            count += 1
        else :
            f_txn_out.write (cols[0] + ' ' + keymap[int(cols[1])] + '\n')

f_load.close()
f_load_out.close()
f_txn.close()
f_txn_out.close()

cmd = 'rm -f ' + out_load_ycsbkey
os.system(cmd)
cmd = 'rm -f ' + out_txn_ycsbkey
os.system(cmd)
