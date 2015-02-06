/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Soham J. Desai 
 * Date         : 14th January 2014
 * Description  : Superscalar Pipeline for Lab2 ECE 6100
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>

extern int32_t PIPE_WIDTH;
extern int32_t ENABLE_MEM_FWD;
extern int32_t ENABLE_EXE_FWD;
extern int32_t BPRED_POLICY;

void shift_registers(Pipeline *p,int move_count);

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Op
 **********************************************************************/

void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
    uint8_t bytes_read = 0;
    bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);

    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
      fetch_op->valid=false;
      p->halt_op_id=p->op_id_tracker;
      return;
    }

    // got an instruction ... hooray!
    fetch_op->valid=true;
    fetch_op->stall=false;
    fetch_op->is_mispred_cbr=false;
    p->op_id_tracker++;
    fetch_op->op_id=p->op_id_tracker;
    
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->tr_file = tr_file_in;
    p->halt_op_id = ((uint64_t)-1) - 3;           

    // Allocated Branch Predictor
    if(BPRED_POLICY){
      p->b_pred = new BPRED(BPRED_POLICY);
    }

    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;

    uint8_t latch_type_i = 0;   // Iterates over Latch Types
    uint8_t width_i      = 0;   // Iterates over Pipeline Width
    for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
        switch(latch_type_i) {
            case 0:
                printf("    FE: ");
                break;
            case 1:
                printf("    ID: ");
                break;
            case 2:
                printf("    EX: ");
                break;
            case 3:
                printf("    MEM: ");
                break;
            default:
                printf(" ---- ");
        }
    }
    printf("\n");
    for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
        for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
            if(p->pipe_latch[latch_type_i][width_i].valid == true) {
	      printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
            } else {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");

}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);
	
	pipe_print_state(p);
	    
}
/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(p->pipe_latch[MEM_LATCH][ii].valid){
      p->stat_retired_inst++;
      if(p->pipe_latch[MEM_LATCH][ii].op_id >= p->halt_op_id){
	p->halt=true;
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_ID(Pipeline *p){
	int forwarding_enabled = ENABLE_EXE_FWD && ENABLE_MEM_FWD; 
//resetting the registers
	int i,j;
	for(i=0;i<PIPE_WIDTH;i++){
		p->pipe_latch[ID_LATCH][i].valid = false;
		p->pipe_latch[FE_LATCH][i].stall = false;
	}	

  int ii,jj,kk;
  for(ii=0; ii<PIPE_WIDTH; ii++){
	Trace_Rec *fe_entry = &(p->pipe_latch[FE_LATCH][ii].tr_entry);
	for(jj=EX_LATCH; jj<=MEM_LATCH; jj++){
	    for(kk=0; kk<PIPE_WIDTH; kk++){	
			Pipeline_Latch *jlatch = &(p->pipe_latch[jj][kk]);
			Trace_Rec *jentry = &(jlatch->tr_entry);

			if(!jlatch->valid){
				continue; // no dependency from this instruction.
			}	  

			if(forwarding_enabled && (jj==MEM_LATCH)){
				continue; // no dependency in the MEM Stage if forwarding enabled	
			}

			if(forwarding_enabled && (jj==EX_LATCH)){
				if(jentry->op_type != OP_LD){	
					continue; //only LD operations have dependency	
				}
			}

			if( (fe_entry->cc_read == 1) && (jentry->cc_write == 1)){
				p->pipe_latch[FE_LATCH][ii].stall = true;
				break;
			}

			if(!jentry->dest_needed){
				continue; // no dependency from this instruction.
			}  
			if(fe_entry->src1_needed && 
						 (fe_entry->src1_reg == jentry->dest)){
				p->pipe_latch[FE_LATCH][ii].stall = true;
				break;
			}
			if(fe_entry->src2_needed && 
					 (fe_entry->src2_reg == jentry->dest)){
				p->pipe_latch[FE_LATCH][ii].stall = true;
				break;
			}	
		}
	  }
	}
	
	//finding stalls due to dependencies within ID stage
	for(i=0;i<PIPE_WIDTH;i++){
		Trace_Rec *i_entry = &(p->pipe_latch[FE_LATCH][i].tr_entry);
		for(j=i+1;j<PIPE_WIDTH;j++){
			Trace_Rec *j_entry = &(p->pipe_latch[FE_LATCH][j].tr_entry);

			if( (i_entry->cc_read == 1) && (j_entry->cc_write == 1)){
				p->pipe_latch[FE_LATCH][i].stall = true;
				break;
			}
			if(!j_entry->dest_needed){
				continue; // no dependency from this instruction.
			}  
			if(i_entry->src1_needed && 
						 (i_entry->src1_reg == j_entry->dest)){
				p->pipe_latch[FE_LATCH][i].stall = true;
				break;
			}
			if(i_entry->src2_needed && 
					 (i_entry->src2_reg == j_entry->dest)){
				p->pipe_latch[FE_LATCH][i].stall = true;
				break;
			}	
		}
	}

	//now we have identified all the stalls. move the non-stalled instructions
	//to ID stage.
	int move_count=0;
	for(i=PIPE_WIDTH-1;i>=0;i--){
		if(!p->pipe_latch[FE_LATCH][i].stall){
			p->pipe_latch[ID_LATCH][i]=p->pipe_latch[FE_LATCH][i];	
			move_count++;
		}else{
			break; // stop moving if we encounter a stall
		}
	}
	//shift the remaining FE_LATCH registers down
	shift_registers(p,move_count);

}


void shift_registers(Pipeline *p,int move_count){
	int i,j;
	for(j=0;j<move_count;j++){
		for(i=PIPE_WIDTH-1;i>0;i--){
			p->pipe_latch[FE_LATCH][i]=p->pipe_latch[FE_LATCH][i-1];
		}
		p->pipe_latch[FE_LATCH][0].valid=false;
	}
}


void print_entry(Trace_Rec *tr_entry,int op_id){
	
	printf("op id : %d\n",op_id);	
	switch(tr_entry->op_type){
		
		case OP_ALU:
			printf("optype : OP_ALU\n");
			break;
		case OP_LD:
			printf("optype : OP_LD\n");
			break;
		case OP_ST:
			printf("optype : OP_ST\n");
			break;
		case OP_CBR:
			printf("optype : OP_CBR\n");
			break;
		case OP_OTHER:
			printf("optype : OP_OTHER\n");
			break;
		default:
			printf("wrong execution path..\n");
	}	

	printf("dest : %" SCNu8 "\n",tr_entry->dest);	
	printf("dest needed : %" SCNu8 "\n",tr_entry->dest_needed);	
	printf("src1_reg : %" SCNu8 "\n",tr_entry->src1_reg);	
	printf("src1 needed : %" SCNu8 "\n",tr_entry->src1_needed);	
	printf("src2_reg : %" SCNu8 "\n",tr_entry->src2_reg);	
	printf("src2 needed : %" SCNu8 "\n",tr_entry->src2_needed);	
	printf("cc_write : %" SCNu8 "\n",tr_entry->cc_write);	
	printf("cc_read : %" SCNu8 "\n",tr_entry->cc_read);	
	printf("\n\n");

}

//--------------------------------------------------------------------//

Pipeline_Latch fetch_op;
int value_present=0;
void pipe_cycle_FE(Pipeline *p){
  int ii;
  //Pipeline_Latch fetch_op;

  for(ii=PIPE_WIDTH-1; ii>=0; ii--){
	//fill the remaining registers after stalls
	if(!p->pipe_latch[FE_LATCH][ii].valid && !p->fetch_cbr_stall){
		if(value_present == 0){
			pipe_get_fetch_op(p, &fetch_op);
		}

		if(BPRED_POLICY){
		  pipe_check_bpred(p, &fetch_op);
		}
		
		// copy the op in FE LATCH
		p->pipe_latch[FE_LATCH][ii]=fetch_op;
		print_entry(&fetch_op.tr_entry,fetch_op.op_id);
		value_present=0;
	}
  }
  
}


//--------------------------------------------------------------------//

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
	p->fetch_cbr_stall=true;
}


//--------------------------------------------------------------------//

