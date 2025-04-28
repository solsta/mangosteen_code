//
// Created by se00598 on 23/02/24.
//
#include "redo_entry_collector.h"
#include <stddef.h>
#include <drmgr.h>
#include "instrumenation_common.h"

void print_reg_values(uintptr_t addr,int val){
    //per_thread_t *data;

    //data = drmgr_get_tls_field(dr_get_current_drcontext(), tls_index);
    //printf("Val: %d Addr: %p\n", val, addr);
}

void print_dest_and_range(uintptr_t dest, intptr_t range){
    //printf("dest: %p range: %p size : %lu\n", dest, range, range - dest);
}

void print_dest_address(uintptr_t addr){
    //printf("Addr: %p\n", addr);
}

void print_progress(){
    printf("Past hash set size and combiner checks\n");
}

void print_counter_value(int size){
    printf("Counter value: %d\n", size);
}

void dump_address_to_overflow_buffer(uintptr_t addr, int size){
    //printf("Dumping hash set addr:%p ; size: %d\n", addr, size);
/*
    unsigned long alignedAddress;
    unsigned long bitmask = ~0UL << MASK_SHIFT;
    unsigned long range = (unsigned long)addr+size;
    alignedAddress = (unsigned long)addr & bitmask;
    overflowBuffer.buffer[overflowBuffer.size] = (void *) alignedAddress;
*/
    // TODO take care of instructions that span multiple 32 aligned blocks.

    overflowBuffer.buffer[overflowBuffer.size] = (void *) addr;
    overflowBuffer.size+=1;
    printf("Dumping complete, entries: %d\n", overflowBuffer.size);

}

void increment_entry_counter(assembly_args *assemblyArgs){
    opnd_t opnd1, opnd2;
    opnd1 = opnd_create_reg(assemblyArgs->counter_pointer_reg);
    opnd2 = OPND_CREATE_INT32(1);
    assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    opnd1 = OPND_CREATE_MEMPTR(assemblyArgs->rsp,0);
    opnd2 = opnd_create_reg(assemblyArgs->counter_pointer_reg);
    assemblyArgs->instr = INSTR_CREATE_mov_st(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
}

void load_counter_in_register(assembly_args *assemblyArgs){
    opnd_t opnd1, opnd2;
    opnd1 = opnd_create_reg(assemblyArgs->rsp);
    opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer,
                               offsetof(per_thread_t, number_of_hash_set_entries));
    instr_t *load_counter_value = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, load_counter_value);

    opnd1 = opnd_create_reg(assemblyArgs->counter_pointer_reg);
    opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->rsp, 0);
    assemblyArgs->instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
}

void check_combiner_state(assembly_args *assemblyArgs){
    opnd_t opnd1, opnd2;
    // Combiner check in progress
    opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
    opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer, offsetof(per_thread_t, combiner));
    //opnd2 = OPND_CREATE_MEMPTR(reg_storing_hash_set_pointer, offsetof(per_thread_t, hash_set_entries));
    instr_t *load_combiner_value = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, load_combiner_value);

    opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
    opnd2 = OPND_CREATE_INT32(0);
    assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    opnd1 = opnd_create_instr(assemblyArgs->restore_registers);
    assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_je, opnd1);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
}

void check_remaining_space_in_hash_set(assembly_args *assemblyArgs) {
    opnd_t opnd1, opnd2;
    load_counter_in_register(assemblyArgs);


#ifdef DISABLE_OVERFLOW_BUFFER
    //  check if count is less that hash set size
    opnd1 = opnd_create_reg(assemblyArgs->counter_pointer_reg);
    opnd2 = OPND_CREATE_INT32(HASH_SET_SIZE);
    assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    opnd1 = opnd_create_instr(assemblyArgs->use_overflow_buffer); // Jump to check duplicate
    assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jge, opnd1);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
#endif
}

void store_address_in_buffer(assembly_args *assemblyArgs) {

    instr_t *insert_stubs_to_cover_given_memory_range = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success = INSTR_CREATE_label(
            assemblyArgs->drcontext);
    instr_t *check_if_stub_is_a_duplicate = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *check_next_index = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *process_any_remaining_stubs = INSTR_CREATE_label(assemblyArgs->drcontext);
    assemblyArgs->restore_registers = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *use_overflow_buffer = INSTR_CREATE_label(assemblyArgs->drcontext);

    assemblyArgs->use_overflow_buffer = use_overflow_buffer;

    opnd_t ref, opnd1, opnd2;
    ref = instr_get_dst(assemblyArgs->memref_instr, assemblyArgs->pos);
    assemblyArgs->ref = ref;

    //-------
    opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
    opnd2 = opnd_create_reg(assemblyArgs->value_at_index);
    assemblyArgs->instr = INSTR_CREATE_xor(assemblyArgs->drcontext, opnd1, opnd2);

    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    check_combiner_state(assemblyArgs);

    // Now get the address
    drutil_insert_get_mem_addr(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->ref,
                               assemblyArgs->reg_storing_destination,
                               assemblyArgs->range);
    //load buffer address
    //drmgr_insert_read_tls_field(assemblyArgs->drcontext, tls_index, assemblyArgs->ilist, assemblyArgs->where,
    //                            assemblyArgs->reg_storing_hash_set_pointer);

    load_counter_in_register(assemblyArgs);

    opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
    opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer, offsetof(per_thread_t, hashSetBypassBuffer));
    instr_t *load_buf_ptr_instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, load_buf_ptr_instr);

    // TODO Handle count > MAX



 //   dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_counter_value, false, 1,
 //                         opnd_create_reg(assemblyArgs->counter_pointer_reg));

    // Now counter_pointer_reg stores a number of elements
    // add count * 16 to buffer address [addr, size]

///
    opnd1 = opnd_create_reg(assemblyArgs->range);
    opnd2 = opnd_create_reg(assemblyArgs->counter_pointer_reg);
    assemblyArgs->instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where,  assemblyArgs->instr);
///
    opnd1 = opnd_create_reg(assemblyArgs->range);
    assemblyArgs->instr = INSTR_CREATE_imul_imm(assemblyArgs->drcontext,opnd1,opnd1, OPND_CREATE_INT32(16));
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where,  assemblyArgs->instr);

    // Need to use a different register
    // copy content of counter_pointer_reg
    // imul, then change subsequent uses

    // TODO check the value calculation here using clean call^

    //dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_counter_value, false, 1,
    //                     opnd_create_reg(assemblyArgs->counter_pointer_reg));


    opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
    opnd2 = opnd_create_reg(assemblyArgs->range);
    assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where,  assemblyArgs->instr);
    //store addr

    opnd1 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer,0);//opnd_create_reg(reg_storing_hash_set_pointer);
    opnd2 = opnd_create_reg(assemblyArgs->reg_storing_destination);
    assemblyArgs->instr = INSTR_CREATE_mov_st(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    // add 8 to reg_storing_hash_set_pointer
    opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
    opnd2 = OPND_CREATE_INT32(8);
    assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where,  assemblyArgs->instr);

    //store size zz
    opnd1 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer,0);//opnd_create_reg(reg_storing_hash_set_pointer);
    opnd2 = OPND_CREATE_INT32(drutil_opnd_mem_size_in_bytes(assemblyArgs->ref, assemblyArgs->memref_instr));
    assemblyArgs->instr = INSTR_CREATE_mov_st(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    increment_entry_counter(assemblyArgs);


    /* Restore scratch registers */
    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where,assemblyArgs->restore_registers);
}

void store_address_in_hash_set(assembly_args *assemblyArgs) {

    instr_t *insert_stubs_to_cover_given_memory_range = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success = INSTR_CREATE_label(
            assemblyArgs->drcontext);
    instr_t *check_if_stub_is_a_duplicate = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *check_next_index = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *process_any_remaining_stubs = INSTR_CREATE_label(assemblyArgs->drcontext);
    assemblyArgs->restore_registers = INSTR_CREATE_label(assemblyArgs->drcontext);
    instr_t *use_overflow_buffer = INSTR_CREATE_label(assemblyArgs->drcontext);

    assemblyArgs->use_overflow_buffer = use_overflow_buffer;

    opnd_t ref, opnd1, opnd2;
    ref = instr_get_dst(assemblyArgs->memref_instr, assemblyArgs->pos);
    assemblyArgs->ref = ref;

    //-------
    opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
    opnd2 = opnd_create_reg(assemblyArgs->value_at_index);
    assemblyArgs->instr = INSTR_CREATE_xor(assemblyArgs->drcontext, opnd1, opnd2);

    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);





///

    check_combiner_state(assemblyArgs);
    check_remaining_space_in_hash_set(assemblyArgs);

    opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
    opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer, offsetof(per_thread_t, hash_set_entries));
    instr_t *load_buf_ptr_instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, load_buf_ptr_instr);
    //At this point reg_storing_hash_set_pointer points to 0th element of hash set

    // Now get the address
    drutil_insert_get_mem_addr(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, ref,
                               assemblyArgs->reg_storing_destination,
                               assemblyArgs->range);

    //dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_dest_address, false,
    //                     1,
    //                     opnd_create_reg(assemblyArgs->reg_storing_destination));

    {
#ifdef STACK_FILTERING
        instr = INSTR_CREATE_mov_ld(drcontext, opnd_create_reg(range), opnd_create_reg(rsp));
        instrlist_meta_preinsert(ilist, where, instr);

        opnd1 = opnd_create_reg(reg_storing_destination);
        opnd2 = opnd_create_reg(range);

        instr_t *compare_instruction = INSTR_CREATE_cmp(drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(ilist, where, compare_instruction);
        opnd_t skip_stack_opnd = opnd_create_instr(restore_registers);
        instr_t *jump_instruction =
                INSTR_CREATE_jcc(drcontext, OP_jle, skip_stack_opnd);
        instrlist_meta_preinsert(ilist, where, jump_instruction);
#endif

    }

    {
        // now calculate range
        //  take a register and load the destination address
        opnd1 = opnd_create_reg(assemblyArgs->range);
        opnd2 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        assemblyArgs->instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        // Increase that register by the get_size
        opnd1 = opnd_create_reg(assemblyArgs->range);
        opnd2 = OPND_CREATE_INT32(drutil_opnd_mem_size_in_bytes(ref, assemblyArgs->memref_instr));
        //opnd2 = OPND_CREATE_INT32(instr_length(assemblyArgs->drcontext, assemblyArgs->memref_instr));
        //opnd2 = OPND_CREATE_INT32(32);
        assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
        /*
        dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_dest_and_range,
                             false, 2,
                             opnd_create_reg(assemblyArgs->reg_storing_destination),
                             opnd_create_reg(assemblyArgs->range));
                             */

//#endif

    }

    {  //Get aligned address
        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_destination);
#if PAYLOAD_SIZE == 256
        //int8_t value = (int8_t)0b11000000;  // Signed 64-bit integer value equivalent to 0xFFFFFFE0
        int8_t value = (int8_t)0b00000000;
#endif
#if PAYLOAD_SIZE == 128
        //int8_t value = (int8_t)0b11000000;  // Signed 64-bit integer value equivalent to 0xFFFFFFE0
        int8_t value = (int8_t)0b10000000;
#endif

#if PAYLOAD_SIZE == 64
        //int8_t value = (int8_t)0b11000000;  // Signed 64-bit integer value equivalent to 0xFFFFFFE0
        int8_t value = 0xC0;
#endif
#if PAYLOAD_SIZE == 32
        int8_t value = 0xE0;  // Signed 64-bit integer value equivalent to 0xFFFFFFE0
#endif
#if PAYLOAD_SIZE == 8
        int8_t value = 0xF8;
#endif
        opnd2 = OPND_CREATE_INT8(value);
        assemblyArgs->instr = INSTR_CREATE_and(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }

    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, insert_stubs_to_cover_given_memory_range);
    {

        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        opnd2 = opnd_create_reg(assemblyArgs->range);
        assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);



        opnd1 = opnd_create_instr(assemblyArgs->restore_registers);
        assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jge, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }

    {//calculate hash set index and store in rax; a % b is equivalent to (b - 1) & a

        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_xor(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = OPND_CREATE_INT32(HASH_SET_SIZE-1);
        assemblyArgs->instr = INSTR_CREATE_mov_imm(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
        //and with di

        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        assemblyArgs->instr = INSTR_CREATE_and(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }

    { //Index calculation
        //1.
        opnd1 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_imul_imm(assemblyArgs->drcontext,opnd1,opnd1, OPND_CREATE_INT32(8));
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }

    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where,try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success);

    // 1. Calculate real offset (RAX*8)
    // 2. ADD RAX to reg_storing_hash_set_pointer
    // 3. load value stored at reg_storing_hash_set_pointer
    // 4. Check if slot is free
    {
        //On subsequent iterations rax has the correct index
        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
        opnd2 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        //3.
        opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
        opnd2 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer,0);
        assemblyArgs->instr = INSTR_CREATE_mov_ld(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        //check if duplicate
        //if not jump to

        //4. Now compare value_at_index with 0
        // TODO TRY TO CHECK IF DUPLICATE FIRST, since 85% duplicates
        opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
        opnd2 = OPND_CREATE_INT32(0);
        assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_instr(check_if_stub_is_a_duplicate); // Jump to check duplicate
        assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jne, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        //store value
        opnd1 = OPND_CREATE_MEMPTR(assemblyArgs->reg_storing_hash_set_pointer,0);//opnd_create_reg(reg_storing_hash_set_pointer);
        opnd2 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        assemblyArgs->instr = INSTR_CREATE_mov_st(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        // TODO add a counter  What are the registers that need/can to be used?
        // Increment entry counter
        {
            increment_entry_counter(assemblyArgs);
          // jump here if number of entries is < number of hash set slots

        }
        // Jump to process_any_remaining_stubs
        opnd1 = opnd_create_instr(process_any_remaining_stubs);
        assemblyArgs->instr = INSTR_CREATE_jmp(assemblyArgs->drcontext, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }
    //Check if we hit a duplicate
    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where, check_if_stub_is_a_duplicate);
    {
        opnd1 = opnd_create_reg(assemblyArgs->value_at_index);
        opnd2 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_instr(check_next_index); // Jump to found duplicate to record duplicates
        assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jne, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_instr(process_any_remaining_stubs);
        assemblyArgs->instr = INSTR_CREATE_jmp(assemblyArgs->drcontext, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
    }
    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where, check_next_index);
    {

        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
        opnd2 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_sub(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = OPND_CREATE_INT32(8);
        assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        // NOW we need to check if rax < hash set size * 8 if so jmp to try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success
        // else set rax to 0
        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = OPND_CREATE_INT32(REAL_HASH_SET_SIZE);
        assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_instr(try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success);
        assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jl, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        //?try to xor rax
        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_xor(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        //Otherwise set rax to 0 and jump to try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success
        opnd1 = opnd_create_reg(assemblyArgs->rax);
        opnd2 = OPND_CREATE_INT32(0);
        assemblyArgs->instr = INSTR_CREATE_mov_imm(assemblyArgs->drcontext,opnd1,opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_instr(try_to_insert_stub_into_given_slot_and_retry_with_higher_index_until_success);
        assemblyArgs->instr = INSTR_CREATE_jmp(assemblyArgs->drcontext,opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

    }
    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where, process_any_remaining_stubs);
    {
        // TODO INVESTIGATE THE FOLLOWING 4 lines
        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_hash_set_pointer);
        opnd2 = opnd_create_reg(assemblyArgs->rax);
        assemblyArgs->instr = INSTR_CREATE_sub(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        opnd2 = OPND_CREATE_INT32(PAYLOAD_SIZE);
        assemblyArgs->instr = INSTR_CREATE_add(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);

        opnd1 = opnd_create_reg(assemblyArgs->reg_storing_destination);
        opnd2 = opnd_create_reg(assemblyArgs->range);
        assemblyArgs->instr = INSTR_CREATE_cmp(assemblyArgs->drcontext, opnd1, opnd2);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);



        opnd1 = opnd_create_instr(assemblyArgs->restore_registers);
        assemblyArgs->instr = INSTR_CREATE_jcc(assemblyArgs->drcontext, OP_jge, opnd1);
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->where, assemblyArgs->instr);
/*
        dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_dest_and_range, false, 2,
                             opnd_create_reg(assemblyArgs->reg_storing_destination), opnd_create_reg(assemblyArgs->range));
*/
        instr_t *jump_instr = INSTR_CREATE_jmp(assemblyArgs->drcontext, opnd_create_instr(insert_stubs_to_cover_given_memory_range));
        instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->instr, jump_instr);
    }
    instr_t *jump_instr = INSTR_CREATE_jmp(assemblyArgs->drcontext, opnd_create_instr(assemblyArgs->restore_registers));
    // Insert the jump instruction before the original call instruction
    instrlist_meta_preinsert(assemblyArgs->ilist, assemblyArgs->instr, jump_instr);

    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where,use_overflow_buffer);

    drutil_insert_get_mem_addr(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, ref, assemblyArgs->reg_storing_destination,
                               assemblyArgs->range);

    //dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where, print_reg_values, false, 2,
    //                     opnd_create_reg(assemblyArgs->reg_storing_destination), opnd_create_reg(assemblyArgs->counter_pointer_reg));
    dr_insert_clean_call(assemblyArgs->drcontext, assemblyArgs->ilist, assemblyArgs->where,
                         dump_address_to_overflow_buffer, false, 2,opnd_create_reg(assemblyArgs->reg_storing_destination),
                         opnd_create_reg(drutil_opnd_mem_size_in_bytes(ref, assemblyArgs->memref_instr)));

    /* Restore scratch registers */
    instrlist_meta_preinsert(assemblyArgs->ilist,assemblyArgs->where,assemblyArgs->restore_registers);
}
