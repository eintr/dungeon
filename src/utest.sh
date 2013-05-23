test_name=$1

function usage() 
{
	echo "sh $0 module_name"
}

flags="cJSON.c -D_GNU_SOURCE -lpthread -lm -g "

llist="aa_llist.c"
bufferlist="aa_bufferlist.c"

case $test_name in
	"llist") 
		gcc $llist $flags -D AA_LLIST_TEST
		;;
	"bufferlist")
		gcc $bufferlist $llist $flags -D AA_BUFFERLIST_TEST
		;;
	*)
		usage
	;;
esac
