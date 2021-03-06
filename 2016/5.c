
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>

#if 1
#define THREADS	4
#define debug(f,a...)	do{}while(0)
#else
#define DEBUG
#define THREADS	1
#define debug(f,a...)	printf("%16s.%-4d."f,__func__, __LINE__,##a)
#endif
#define error(f,a...)	printf("%16s.%-4d."f,__func__, __LINE__,##a)

#define list_init(l)		{(l)->next = (l)->prev = (l);}
#define list_add_before(to,new)	{ \
	(to)->prev->next = new; \
	(new)->prev = (to)->prev; \
	(to)->prev = new; \
	(new)->next = to; \
}
#define list_remove(l)		{ \
	(l)->prev->next = (l)->next; \
	(l)->next->prev = (l)->prev; \
	(l)->next = (l)->prev = NULL; \
}

struct list
{
	struct list *next;
	struct list *prev;

	void *data;
};

struct threadpool_arg
{
};

struct threadpool_control
{
	struct list running;
	pthread_cond_t jobdone;
	pthread_mutex_t running_lock;
};

struct threadpool_control *tp_new_control (int max_thread, int (*thread_func)(void *))
{
}


struct jobcontrol;

struct jobarg
{
	int number;
	int running;
	int time_consumed;
	struct jobcontrol *jc;
	unsigned long long result;

	int N;

	int *P, *W;
	int pos_w[50];
	int count_w;
};

struct jobcontrol
{
	FILE *in, *out;

	pthread_cond_t jobdone;
	pthread_mutex_t lock;

	int next_job;
	struct jobarg *arg;
	pthread_t *tid;
	int T;
};

/* return in ms */
static int now (void)
{
	struct timeval tv;

	gettimeofday (&tv, NULL);

	return tv.tv_sec*1000 + tv.tv_usec/1000;
}

static int comp_pow (const void *a, const void *b, void *p)
{
	int *p1, *p2;
	int *P = p;

	p1 = (void*)a;
	p2 = (void*)b;

	//return *p1-*p2;
	return P[*p2]-P[*p1];
}

int analize_pos (struct jobarg *j,
		int start, int count,
		int *P, int *W, int *wpos,
		int *mpos, int *mpos_count,
		int *_max_man_power,
		int *_max_man_pos)
{
	int i;
	int ret;
	int max_man_power = 0;
	int max_man_pos = 0;

	int local_man_pos = -1;
	int local_man_power = 0;
	int local_man_count = 0;

	ret = 0;
	for (i=0; i<count; i++)
	{
		int pos;

		if (!W[i])
		{
			//debug ("  man on %d, p%d\n", i+start, P[i]);

			/* its man */
			if (P[i] > max_man_power)
			{
				max_man_power = P[i];
				max_man_pos = i;
			}

			//if (P[i] > local_man_power)
			//{
			//	local_man_power = P[i];
			//	local_man_pos = i;
			//}
			continue;
		}

		//debug ("woman on %d, p%d\n", i+start, P[i]);

		/* its woman */
		//if (local_man_pos >= 0)
		//{
			//debug ("update local max position %d, p%d\n",
			//		local_man_pos+start, local_man_pos);

		//	mpos[local_man_count] = local_man_pos;
		//	local_man_count ++;

		//	local_man_power = 0;
		//	local_man_pos = -1;
		//}

		wpos[ret] = i;
		ret ++;
	}

	//if (local_man_pos >= 0)
	//{
	//	mpos[local_man_count] = local_man_pos;
	//	local_man_count ++;
	//}

	*mpos_count = local_man_count;
	*_max_man_power = max_man_power;
	*_max_man_pos = max_man_pos;

	qsort_r (wpos, ret, sizeof (wpos[0]), comp_pow, P);
	//qsort_r (mpos, local_man_count, sizeof (mpos[0]), comp_pow, P);

	return ret;
}

static int comp_int (const void *a, const void *b)
{
	int *p1, *p2;

	p1 = (void*)a;
	p2 = (void*)b;

	return *p2-*p1;
	//return P[*p2]-P[*p1];
}

int possible_max (int first, int second, int count, int *P, int *W)
//int possible_max (int count, int *P, int *W)
{
	int i;
	int wbuflen = count>50?50:count;
	int mbuflen = count;

	int wbuf[wbuflen];
	int mbuf[mbuflen];
	int wbuf_size = 0;
	int mbuf_size = 0;

	int max_matching;
	int pow;

	for (i=0; i<count; i++)
	{
		if (i == first || i == second)
			continue;

		if (W[i])
			wbuf[wbuf_size++] = P[i];
		else
			mbuf[mbuf_size++] = P[i];
	}

	qsort (wbuf, wbuf_size, sizeof (wbuf[0]), comp_int);
	qsort (mbuf, mbuf_size, sizeof (mbuf[0]), comp_int);

	if (mbuf_size < wbuf_size)
		max_matching = mbuf_size;
	else
		max_matching = wbuf_size;

	pow = 0;
	for (i=0; i<max_matching; i++)
	{
		pow += wbuf[i] * mbuf[i];
		//debug ("pow %d, %dx%d\n", pow, wbuf[i], mbuf[i]);
	}

	return pow;
	//return INT_MAX;
}

int get_possible (int count, int first, int second, int *P, int *W)
{
	int subAsize, subBsize;
	int maxA, maxB;
	int power;

	power = possible_max (first, second, count, P, W);
	power += P[first]*P[second];

	return power;

#if 0
	if (first > second)
	{
		int t;
		t = first;
		first = second;
		second = t;
	}

	subAsize = second-first-1;
	subBsize = count - subAsize - 2;

	maxA = maxB = 0;
	if (subAsize > 1)
		maxA = possible_max (subAsize, P+first+1, W+first+1);

	if (subBsize > 1)
	{
		int tailsize;

		tailsize = count-first-subAsize-2;

		if (first == 0)
		{
			maxB = possible_max ( subBsize,
					P + second + 1, W + second + 1);
		}
		else if (tailsize == 0)
		{
			maxB = possible_max ( subBsize,
					P, W);
		}
		else
		{
			int *subP, *subW;

			subP = malloc (subBsize*sizeof (subP[0]));
			subW = malloc (subBsize*sizeof (subW[0]));

			memcpy (subP, P+first+subAsize+2, tailsize*sizeof(subP[0]));
			memcpy (subW, W+first+subAsize+2, tailsize*sizeof(subW[0]));
			memcpy (subP+tailsize, P, first*sizeof(subP[0]));
			memcpy (subW+tailsize, W, first*sizeof(subW[0]));

			maxB = possible_max ( subBsize,
					subP, subW);

			free (subP);
			free (subW);
		}
	}

	return P[first]*P[second] + maxA + maxB;
#endif
}

static int max_power (struct jobarg *j, int depth, int start, int count, int *P, int *W);

int get_power (struct jobarg *j, int depth, int try, int total,
		int start, int count,
		int first, int second, int *P, int *W)
{
	int subAsize, subBsize;
	int maxA, maxB;
	int power;

	if (first > second)
	{
		int t;
		t = first;
		first = second;
		second = t;
	}

	subAsize = second-first-1;
	subBsize = count - subAsize - 2;
	debug ("%4d-%4d %d %d/%d check %d and %d, sub section count A %d, B %d\n",
			start, start + count,
			depth, try, total,
			start + first,
			start + second,
			subAsize, subBsize);
	//if (j->number == 1 && depth <= 2)
	//	printf ("%d. depth%d, %d/%d\n", j->number, depth, try, total);

	maxA = maxB = 0;
	if (subAsize > 1)
		maxA = max_power (j, depth,
				start + first + 1, subAsize,
				P+first+1, W+first+1);

	if (subBsize > 1)
	{
		int tailsize;

		tailsize = count-first-subAsize-2;

		if (first == 0)
		{
			maxB = max_power (j, depth,
					start + second + 1, subBsize,
					P + second + 1, W + second + 1);
		}
		else if (tailsize == 0)
		{
			maxB = max_power (j, depth,
					start, subBsize,
					P, W);
		}
		else
		{
			int *subP, *subW;

			subP = malloc (subBsize*sizeof (subP[0]));
			subW = malloc (subBsize*sizeof (subW[0]));

			memcpy (subP, P+first+subAsize+2, tailsize*sizeof(subP[0]));
			memcpy (subW, W+first+subAsize+2, tailsize*sizeof(subW[0]));
			memcpy (subP+tailsize, P, first*sizeof(subP[0]));
			memcpy (subW+tailsize, W, first*sizeof(subW[0]));

			maxB = max_power (j, depth,
					start + second + 1, subBsize,
					subP, subW);

			free (subP);
			free (subW);
		}
	}

	debug ("%4d-%4d %d %d/%d check %d and %d, %d+%d+%d = %d\n",
			start, start + count,
			depth, try, total,
			start + first,
			start + second,
			P[first]*P[second], maxA, maxB,
			P[first]*P[second] + maxA + maxB);

	return P[first]*P[second] + maxA + maxB;
}

int max_power (struct jobarg *j, int depth, int start, int count, int *P, int *W)
{
	int first;
	int i, w, m;
	int max = 0;
	int w_pos[50];
	int m_pos[51];
	int w_count;
	int m_count = 0;
	int max_man_power = 0;
	int max_man_pos = 0;
	int try = 0;

	w_count = analize_pos (j, start, count, P, W, w_pos, m_pos, &m_count, &max_man_power, &max_man_pos);
	if (w_count == 0)
	{
		//debug ("%4d-%4d no women\n",
		//		start, start + count);
		return 0;
	}
	if (w_count == count)
	{
		//debug ("%4d-%4d no man\n",
		//		start, start + count);
		return 0;
	}
	if (w_count == 1)
	{
		//debug ("%4d-%4d single woman. give max man %d(%dx%d)\n",
		//		start, start + count,
		//		P[w_pos[0]] * max_man_power,
		//		P[w_pos[0]], max_man_power);
		return P[w_pos[0]] * max_man_power;
	}

#ifdef DEBUG
	{
		char buf[count*5+1];
		int off;

#if 0
		off = 0;
		for (i=0; i<count; i++)
			off += sprintf (buf+off, " %4d", i+start);

		debug ("%4d-%4d %d number%*s%s\n",
				start, start + count, depth,
				5*start, "", buf);

		off = 0;
		for (i=0; i<count; i++)
			off += sprintf (buf+off, " %4d", P[i]);

		debug ("%4d-%4d %d power %*s%s\n",
				start, start + count, depth,
				5*start, "", buf);
#endif

		off = 0;
		for (i=0; i<count; i++)
			//off += sprintf (buf+off, " %4d", W[i]);
			off += sprintf (buf+off, "%d", W[i]);

		debug ("%4d-%4d %d woman %*s%s\n",
				start, start + count, depth,
				//5*start, "", buf);
				1*start, "", buf);
	}
#endif

	//for (m=0; m<m_count; m++)
	//m = 0;
	{
		int second;

		//first = m_pos[m];
		first = max_man_pos;

		for (w=0; w<w_count; w++)
		{
			int power;

			second = w_pos[w];

			if (max > 0)
			{
				int possible;
				possible = get_possible (count,
						first, second,
						P, W);
				if (possible <= max)
				{
					debug ("%4d-%4d %d %d/%d possible %d-%d %d < cur %d. break\n",
							start, start + count,
							depth, try, w_count,
							first+start, second+start,
							possible, max);
					break;
				}
			}

			power = get_power (j, depth+1,
					try, w_count,
					start, count, first, second, P, W);
			if (power < 0)
				break;

			if (power > max)
				max = power;

			try ++;
		}
	}

	debug ("%4d-%4d %d max %d\n",
			start, start + count,
			depth, max);

	return max;
}

static void * jobthread (void *arg)
{
	struct jobarg *j = arg;
	int start = now ();

	/* do job */
	//if (j->number == 15)
	j->result = max_power (j, 0, 0, j->N, j->P, j->W);

	j->time_consumed = now () - start;

	/* mark we done */
	pthread_mutex_lock (&j->jc->lock);
	j->running = 0;
	printf ("%d. %llu w%d, %dms\n", j->number, j->result, j->count_w, j->time_consumed);
	pthread_mutex_unlock (&j->jc->lock);
	pthread_cond_signal (&j->jc->jobdone);

	/* cleanup not used memory */
	free (j->P);
	free (j->W);

	return 0;
}

static int more_job (struct jobcontrol *jc)
{
	struct jobarg *j;
	int jn;

	int a;

	if (jc->next_job >= jc->T)
		return 0;
	jn = jc->next_job ++;

	j = jc->arg + jn;

	fscanf (jc->in, "%d\n", &j->N);

	j->P = calloc (j->N, sizeof (j->P[0]));
	for (a=0; a<j->N; a++)
	{
		int p;

		p = 0;
		fscanf (jc->in, "%d", &p);

		j->P[a] = p;
	}


	j->W = calloc (j->N, sizeof (j->W[0]));
	for (a=0; a<j->N; a++)
	{
		int s;

		s = 0;
		fscanf (jc->in, "%d", &s);

		j->W[a] = !!s;
		if (!!s)
		{
			j->pos_w[j->count_w] = a;
			j->count_w ++;
		}
	}
	printf ("%d. new job. N %d w%d\n", jn, j->N, j->count_w);

	/* set jobthread argument and run the thread */
	j->number = jn;
	j->running = 1;
	j->jc = jc;

	pthread_create (jc->tid+jn, NULL, jobthread, jc->arg+jn);
	pthread_detach (jc->tid[jn]);

	return 0;
}

int now_running (struct jobcontrol *jc)
{
	int i;
	int running = 0;

	for (i=0; i<jc->T; i++)
		if (jc->arg[i].running)
			running ++;

	return running;
}

int main (int argc, char **argv)
{
	char *_in, *_out, *p;
	struct jobcontrol _jc = {0,};
	struct jobcontrol *jc = &_jc;
	int i;

	if (argc < 2)
	{
		error ("no input\n");
		return 1;
	}

	_in = argv[1];
	_out = malloc (strlen (_in) + 5);
	strcpy (_out, _in);
	p = strrchr (_out, '.');
	if (!p)
		p = _out+strlen (_in);
	strcpy (p, ".out");

	debug ("in %s, out %s\n", _in, _out);
	jc->in = fopen (_in, "r");
	jc->out = fopen (_out, "w");
	if (!jc->in || !jc->out)
	{
		error ("no input or output\n");
		return 1;
	}

	pthread_mutex_init (&jc->lock, NULL);
	pthread_cond_init (&jc->jobdone, NULL);

	fscanf (jc->in, "%d\n", &jc->T);
	printf ("T %d\n", jc->T);

	jc->arg = calloc (jc->T, sizeof (jc->arg[0]));
	jc->tid = calloc (jc->T, sizeof (jc->tid[0]));
	if (!jc->arg || !jc->tid)
	{
		error ("no mem for arg or tid\n");
		return 1;
	}

	while (1)
	{
		int running;

		pthread_mutex_lock (&jc->lock);
		running = now_running (jc);
		if (running >= THREADS)
			pthread_cond_wait (&jc->jobdone, &jc->lock);
		else if (jc->next_job >= jc->T)
		{
			if (running == 0)
			{
				debug ("all done.\n");
				pthread_mutex_unlock (&jc->lock);
				break;
			}

			pthread_cond_wait (&jc->jobdone, &jc->lock);
		}
		pthread_mutex_unlock (&jc->lock);

		more_job (jc);
	}

	for (i=0; i<jc->T; i++)
	{
		struct jobarg *j = jc->arg+i;

		printf ("%d. %llu w%d, %dms\n", j->number, j->result, j->count_w, j->time_consumed);
		fprintf (jc->out, "%llu\n", jc->arg[i].result);
	}

	return 0;
}

