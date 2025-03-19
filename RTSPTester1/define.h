
#pragma once

unsigned char* rmemmem(unsigned char* p_src, int src_len, unsigned char* p_trg, int trg_len)
{
	unsigned char* p = nullptr;
	int i;
	int j;
	p = p_src;
	for (i = 0; i < src_len - trg_len + 1; i++)
	{
		if (*p == *p_trg)
		{
			for (j = 1; j < trg_len; j++)
			{
				if (p[j] != p_trg[j])
				{
					break;
				}
			}
			if (j == trg_len)
			{
				return p;
			}
		}
		p++;
	}
	return NULL;
}