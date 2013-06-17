test_name=$1

function usage() 
{
	echo "sh $0 module_name"
}

flags="cJSON.c -D_GNU_SOURCE -lpthread -lm -g -Wall"

llist="aa_llist.c"
bufferlist="aa_bufferlist.c"
hashtable="aa_hasht.c"
memvec="aa_memvec.c"
log="aa_log.c"
dict="aa_state_dict.c"

case $test_name in
	"llist") 
		gcc $llist $flags -D AA_LLIST_TEST
		;;
	"bufferlist")
		gcc $bufferlist $llist $flags -D AA_BUFFERLIST_TEST
		;;
	"hashtable")
		gcc $hashtable $memvec $log $flags -D AA_HASH_TEST
		;;
	"dict")
		gcc $dict $hashtable $memvec $log $flags -D AA_DICT_TEST
		;;
	*)
		usage
		;;
esac
