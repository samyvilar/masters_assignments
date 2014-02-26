//
//  main.c
//  adv_data_structures_hw_4
//
//  Created by Samy Vilar on 11/28/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned long long unsigned_word_type; /* UPDATE MAX_PRIME if working with 32 bit words! */
typedef unsigned char bit_field_block_type; /* you could improve performance by using word blocks instead of char blocks. */
#define MAX_PRIME 4294967291
//#define MAX_PRIME 65521 /* if using 32 bit unsigned ints */


const unsigned_word_type
        number_of_bits_per_word = (sizeof(bit_field_block_type) * 8);


/* masks to update bit
 * use bitwise or operator to set it to 1,
 * use bitwise and operator to get a single bit.
 * setting the bit to 0 would require inverting
 * the mask and applying the bitwise and operator, not needed here ... */
static unsigned_word_type
        masks[(sizeof(bit_field_block_type) * 8)] = {0};

/* Set of macros to allow us to work with individual bits from our field. */

/* get the location of the word which holds the ith bit  */
#define word_index(index) (index/number_of_bits_per_word)

/* get the location of the ith bit withing a word. */
#define bit_index(index) (index % number_of_bits_per_word)

/* get a word from our bit field */
#define word(bit_field, index) (bit_field[word_index(index)])

/* get bit, either returns 0 if its zero or its current value depending on its location in the word, (1, 2, 4, 8 ...)*/
#define get_bit(bit_field, index) (word(bit_field, index) & masks[bit_index(index)])

/* set bit at index within our bit field to 1. */
#define set_bit_to_1(bit_field, index) (bit_field[word_index(index)] |= masks[bit_index(index)])

/* Initialize our masks ... */
void initialize_masks()
{
    unsigned_word_type index = number_of_bits_per_word;
    while (index--)
        masks[index] = (unsigned_word_type)1 << index; /* set each bit by shifting accordingly. */
}

typedef struct hash_function_parameters_type
{
    struct hash_function_parameters_type
            *coefficients;

    unsigned_word_type
            value;

} hash_function_parameters_type;

/* some micros to control the way we deal with this structure.  */
#define next_parameter(hash_func_parameter) (*(hash_function_parameters_type **)hash_func_parameter)
#define set_next_parameter(hash_func_parameter, next_param) (next_parameter(hash_func_parameter) = next_param)
#define value(hash_func_parameter) (hash_func_parameter->value)
#define set_value(hash_func_parameter, param_value) (value(hash_func_parameter) = param_value)
#define set_coefficient_a set_value
#define table_size(hash_func_parameter) value(hash_func_parameter)
#define set_table_size set_value
#define coefficient_b(hash_func_parameter) value(next_parameter(hash_func_parameter))
#define set_coefficient_b(hash_func_parameter, coefficient) (coefficient_b(hash_func_parameter) = coefficient)
#define coefficient_a value
#define initial_coefficient_a(hash_func_parameter) next_parameter(next_parameter(hash_func_parameter))

/* the size of the allocation block use higher values if expecting to hash very long strings... */
#define ALLOCATION_BLOCK_SIZE 128

/* A pre-allocated blocks of parameters used for the hash function. */
static hash_function_parameters_type
        *allocated_block = NULL,
        *max_block       = NULL; /* address of last element withing our block. */

/* return a single hash_func_parameter object with all fields zero out, allocates new block if exhausted. */
hash_function_parameters_type *allocate_node()
{
    if (allocated_block == max_block)   /* if we don't have any pre-allocated nodes left, or initially empty, allocate nodes. */
    {
        max_block = malloc(sizeof(hash_function_parameters_type) * ALLOCATION_BLOCK_SIZE + sizeof(void *));
        *(hash_function_parameters_type **)&max_block[ALLOCATION_BLOCK_SIZE] = allocated_block; /* save previous block. */
        allocated_block = max_block; /* set new block. */
        max_block = allocated_block + ALLOCATION_BLOCK_SIZE; /* locate last unavailable node */
        memset(allocated_block, 0, sizeof(hash_function_parameters_type) * ALLOCATION_BLOCK_SIZE);
        /* zero out the whole block, also move pages from vm pool into main memory, if not present. */
    }

    return allocated_block++; /* return current address, increment for next item. */
}



/*
    Bloom Filter structures use to keep track of whether a string is in a giving set.
    It supports varying number of bitfields all of whose size may be set up to but not including MAX_PRIME.
 */
typedef struct bloom_filter_type
{
    bit_field_block_type
            **bit_fields;       /* set of bit fields. */

    unsigned_word_type
        number_of_bit_fields,   /* number of bit-fields */
        size_of_each_bit_field, /* the size of each bit-field in bits .... */
        (*hashing_function)(char *, hash_function_parameters_type *);
        /* the hashing function use to determine the hash or index of value withing our bit field */

    hash_function_parameters_type
            **hashing_function_parameters; /* set of hash_function parameters one per bit-field. */

} bloom_filter_type;
/* some macros to deal with this structure ... */
#define bit_fields(bloom_filter) (*(bit_field_block_type ***)bloom_filter)
#define set_bit_fields(bf, btf) (bit_fields(bf) = btf)
#define number_of_bit_fields(bf) (bf->number_of_bit_fields)
#define set_number_of_bit_fields(bf, value) (number_of_bit_fields(bf) = value)
#define size_of_each_bit_field(bloom_filter) (bloom_filter->size_of_each_bit_field)
#define set_size_of_each_bit_field(bf, value) (size_of_each_bit_field(bf) = value)
#define hashing_function(bloom_filter) (bloom_filter->hashing_function)
#define set_hashing_function(bf, func) (hashing_function(bf) = func)
#define hashing_function_parameters(bf) (bf->hashing_function_parameters)
#define set_hashing_function_parameters(bf, value) (hashing_function_parameters(bf) = value)




/* Family of Universal hashing function, for null terminated strings
 * it takes in a link list of parameters, where
 * the first is the table size
 * the second is coefficient_b randomly chosen from [0, RAND()]
 * and the third and onward are coefficient_a each randomly chosen from [0, RAND()]
 * where RAND() returns a value from  [0, RAND_MAX]
 * RAND_MAX is system dependent, if integer is 32 bits it should 2147483647 or 0x7fffffff largest positive integer.
 * all have to be less than MAX_PRIME.
 * note that we need as many coefficient_a as the length of the string
 * shorter strings simply are interpret as being padded by 0s so the hash values aren't affected when we increment the list.
 *
 * The function using python notation.
 * hash(string) = (sum( (char_value * coefficient_as[index]) % MAX_PRIME
 *                          for index, char_value in enumerate(string)
 *                    ) + coefficient_b) % table_size
 */
unsigned_word_type universal_hash_function(
        char *char_value,
        hash_function_parameters_type *hash_func_parameter)
{
    unsigned_word_type
            max_value = table_size(hash_func_parameter),
            sum       = coefficient_b(hash_func_parameter);

    hash_func_parameter = initial_coefficient_a(hash_func_parameter);
    char_value--; /* subtract the pointer by one so every time we begin at the loop we just need increment and check it */
    /* note that ++ is applied on the pointer. */
    while (*++char_value) /* while we haven't read the null character, '\0'. */
    {
        if (!next_parameter(hash_func_parameter)) /* if we need a new coefficient_a add it */
        {
            set_next_parameter(hash_func_parameter, allocate_node());
            set_coefficient_a(next_parameter(hash_func_parameter), ((unsigned_word_type)rand() % MAX_PRIME));
        }
        sum += (coefficient_a(hash_func_parameter) *  (unsigned_word_type)*char_value) % MAX_PRIME;

        hash_func_parameter = next_parameter(hash_func_parameter); /* move to next parameter. */
    }

    return (sum % max_value);
}

/*
    Create and return a set of parameters as a newly selected PRF design to work with strings,
    initially assumes that all strings are of length up to 1.
 */
hash_function_parameters_type *get_new_hashing_parameters(unsigned_word_type table_size)
{

    hash_function_parameters_type *hashing_func_parameters = allocate_node();

    set_next_parameter(hashing_func_parameters , allocate_node());                  /* second node to hold coefficient b*/
    set_next_parameter(next_parameter(hashing_func_parameters ), allocate_node());  /* third node for coefficient a */

    set_table_size(hashing_func_parameters, table_size);                            /* set coefficient the table size */
    set_coefficient_b(hashing_func_parameters, (rand() % MAX_PRIME));               /* set coefficient b */

    set_value(next_parameter(next_parameter(hashing_func_parameters)), (rand() % MAX_PRIME)); /* set coefficient a */

    return hashing_func_parameters;

}

/*
    Create a new bloom filter structure.
        @number_of_bit_fields: the number of bit-fields to use.
        @size_of_each_bit_field: the size of each bitfield in bits.
 */
bloom_filter_type *create_bloom_filter(
        unsigned_word_type number_of_bit_fields,
        unsigned_word_type size_of_each_bit_field
    )
{
    if (size_of_each_bit_field > MAX_PRIME)
    {
        printf("The bit field size cannot exceed MAX_PRIME!\n");
        exit(-1);
    }

    bloom_filter_type *bloom_filter = malloc(sizeof(bloom_filter_type)); /* create a new struct ... */

    if (!masks[0])
        initialize_masks(); /* initialized masks, if we haven't. */

    /* set initial properties ... */
    set_number_of_bit_fields(bloom_filter, number_of_bit_fields);
    set_size_of_each_bit_field(bloom_filter, size_of_each_bit_field);
    set_hashing_function(bloom_filter, universal_hash_function);


    /* allocate each bitfield which itself has an accompanying set of hashing parameters. */
    bit_field_block_type **bitfields =
            malloc(sizeof(bit_field_block_type *) * number_of_bit_fields);
    set_bit_fields(bloom_filter, bitfields);

    hash_function_parameters_type **hashing_function_parameters =
            malloc(sizeof(hash_function_parameters_type **) * number_of_bit_fields);
    set_hashing_function_parameters(bloom_filter, hashing_function_parameters);

    unsigned_word_type number_of_bit_field_blocks =
            (size_of_each_bit_field/number_of_bits_per_word)
                + (1 ? (size_of_each_bit_field % sizeof(bit_field_block_type)) : 0);
                /* add an extra element if the bitfield size isn't evenly divisible. */

    while (number_of_bit_fields--)
    {
        *bitfields++ = calloc(number_of_bit_field_blocks, sizeof(bit_field_block_type));
        *hashing_function_parameters++ = get_new_hashing_parameters(size_of_each_bit_field);
    }

    return bloom_filter;
}



/*
    As predefined by the assignment:
        Implement a Bloom filter for 2,000,000 strings with an error rate of less than 3%,
        using only 2Mbyte of memory.
        To achieve this, you create eight bit arrays, each of 2,000,000 bits (that is, 250,000 char).
        For each of these, you select a random hash function hi from a universal family.
        To insert a string s, you set the hi(S)-th bit to one in the i-th bit array, for i = 0,...,7.
        To query whether a string q is contained in the set,
        you check whether hi(q) is one in the i-th bit array, for all i.
 */
typedef bloom_filter_type bf_t;
bf_t * create_bf()
{   return create_bloom_filter(8, 2000000); }

/*
    As predefined by the assignment:
        inserts the string *s into the Bloom filter *b.
 */
void insert_bf(bf_t *bloom_filter, char *string)
{
    unsigned_word_type
        bit_field_index = number_of_bit_fields(bloom_filter),
        hash;

    while (bit_field_index--)
    {
        hash = hashing_function(bloom_filter)(string, hashing_function_parameters(bloom_filter)[bit_field_index]);

        set_bit_to_1(
            bit_fields(bloom_filter)[bit_field_index],
            hash);
    }
}

/*
    As predefined by the assignment:
        returns 1 if the string *q is accepted by the
        Bloom filter, and 0 else.
 */
int is_element(bf_t *bloom_filter, char *string)
{
    unsigned_word_type
            bit_field_index = number_of_bit_fields(bloom_filter),
            hash;

    while (bit_field_index--)
    {
        hash = hashing_function(bloom_filter)(string, hashing_function_parameters(bloom_filter)[bit_field_index]);

        if (!get_bit(bit_fields(bloom_filter)[bit_field_index], hash))
            return 0;
    }
    return 1;
}


/* GIVING TESTS .... *****************************************************/
void sample_string_A(char *s, long i)
{  s[0] = (char)(1 + (i%254));
    s[1] = (char)(1 + ((i/254)%254));
    s[2] = (char)(1 + (((i/254)/254)%254));
    s[3] = 'a';
    s[4] = 'b';
    s[5] = (char)(1 + ((i*(i-3)) %217));
    s[6] = (char)(1 + ((17*i+129)%233 ));
    s[7] = '\0';
}
void sample_string_B(char *s, long i)
{  s[0] = (char)(1 + (i%254));
    s[1] = (char)(1 + ((i/254)%254));
    s[2] = (char)(1 + (((i/254)/254)%254));
    s[3] = 'a';
    s[4] = (char)(1 + ((i*(i-3)) %217));
    s[5] = (char)(1 + ((17*i+129)%233 ));
    s[6] = '\0';
}
void sample_string_C(char *s, long i)
{  s[0] = (char)(1 + (i%254));
    s[1] = (char)(1 + ((i/254)%254));
    s[2] = 'a';
    s[3] = (char)(1 + ((i*(i-3)) %217));
    s[4] = (char)(1 + ((17*i+129)%233 ));
    s[5] = '\0';
}
void sample_string_D(char *s, long i)
{  s[0] = (char)(1 + (i%254));
    s[1] = (char)(1 + ((i/254)%254));
    s[2] = (char)(1 + (((i/254)/254)%254));
    s[3] = 'b';
    s[4] = 'b';
    s[5] = (char)(1 + ((i*(i-3)) %217));
    s[6] = (char)(1 + ((17*i+129)%233 ));
    s[7] = '\0';
}
void sample_string_E(char *s, long i)
{  s[0] = (char)(1 + (i%254));
    s[1] = (char)(1 + ((i/254)%254));
    s[2] = (char)(1 + (((i/254)/254)%254));
    s[3] = 'a';
    s[4] = (char)(2 + ((i*(i-3)) %217));
    s[5] = (char)(1 + ((17*i+129)%233 ));
    s[6] = '\0';
}



int main()
{  long i,j;
    bf_t * bloom;
    bloom = create_bf();
    printf("Created Filter\n");


    for( i= 0; i< 1450000; i++ )
    {  char s[8];
        sample_string_A(s,i);
        insert_bf( bloom, s );
    }
    for( i= 0; i< 500000; i++ )
    {  char s[7];
        sample_string_B(s,i);
        insert_bf( bloom, s );
    }
    for( i= 0; i< 50000; i++ )
    {  char s[6];
        sample_string_C(s,i);
        insert_bf( bloom, s );
    }
    printf("inserted 2,000,000 strings of length 8,7,6.\n");

    for( i= 0; i< 1450000; i++ )
    {  char s[8];
        sample_string_A(s,i);
        if( is_element( bloom, s ) != 1 )
        {  printf("found negative error (1)\n"); exit(0); }
    }
    for( i= 0; i< 500000; i++ )
    {  char s[7];
        sample_string_B(s,i);
        if( is_element( bloom, s ) != 1 )
        {  printf("found negative error (2)\n"); exit(0); }
    }
    for( i= 0; i< 50000; i++ )
    {  char s[6];
        sample_string_C(s,i);
        if( is_element( bloom, s ) != 1 )
        {  printf("found negative error (3)\n"); exit(0); }
    }
    j = 0;
    for( i= 0; i< 500000; i++ )
    {  char s[8];
        sample_string_D(s,i);
        if( is_element( bloom, s ) != 0 )
            j+=1;
    }
    for( i= 0; i< 500000; i++ )
    {  char s[7];
        sample_string_E(s,i);
        if( is_element( bloom, s ) != 0 )
            j+=1;
    }
    printf("Found %li positive errors out of 1,000,000 tests.\n",j);
    printf("Positive error rate %f%%.\n", (float)j/10000.0);

    return 0;
}
