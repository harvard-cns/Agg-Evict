struct rte_hash *hash;
struct rte_hash_parameters params;
void *data;
int key;
int d;
int ret;
bzero (&params,sizeof (params));

params.name = NULL;
params.entries = 500;
params.key_len = sizeof (int);
params.hash_func = rte_jhash;
params.hash_func_init_val = 0;

hash = rte_hash_create (&params);
if (!hash) {
    fprintf (stderr,"rte_hash_create failed\n");
    return;
}   

key = 0;
data = NULL;
key = 1;  
d = 1;
//add 1/1 to hash table
rte_hash_add_key_data (hash,&key,(void *) (long) d); 
key = 2;
d = 2;
//add 2/2 to hash table
rte_hash_add_key_data (hash,&key,(void *) (long) d); 

key = 2;
//I want to lookup with key = 2
//But it failed.
ret = rte_hash_lookup_data (hash,&key,&data);
if (ret < 0) {
    if (ret == ENOENT) {
        fprintf (stderr,"find failed\n");
        return;
    }   

    if (ret == EINVAL) {
        fprintf (stderr,"parameter invalid");
        return;
    }
    fprintf (stderr,"lookup failed");
    return;
}