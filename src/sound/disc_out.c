/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@esplexo.co.uk)                       */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DSO_OUTPUT - Sinewave generator source code                          */
/*                                                                      */
/************************************************************************/

struct dso_output_context
{
	int16_t left;
	int16_t right;
};

/************************************************************************/
/*                                                                      */
/* Usage of node_description values for step function                   */
/*                                                                      */
/* input0    - Left channel output value                                */
/* input1    - Right channel output value                               */
/* input2    - Volume setting (static)                                  */
/* input3    - NOT USED                                                 */
/* input4    - NOT USED                                                 */
/* input5    - NOT USED                                                 */
/*                                                                      */
/************************************************************************/
int dso_output_step(struct node_description *node)
{
	/* We ALWAYS work in stereo here, let the stream update decide if its mono/stereo output */
	struct dso_output_context *context;
	context=(struct dso_output_context*)node->context;
	context->left=(int16_t)node->input0;
	context->right=(int16_t)node->input1;
	return 0;
}

int dso_output_init(struct node_description *node)
{
	/* Allocate memory for the context array and the node execution order array */
	if((node->context=(struct dso_output_context*)malloc(sizeof(struct dso_output_context)))==NULL)
	{
		return 1;
	}
	else
	{
		/* Initialise memory */
		memset(node->context,0,sizeof(struct dso_output_context));
	}

	return 0;
}



