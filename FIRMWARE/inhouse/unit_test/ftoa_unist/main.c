#include <stdio.h>
#include <string.h>
#include <math.h>

typedef unsigned char u8;
typedef signed   char s8;
typedef char          p8;
typedef float         float32;
typedef int           s32;
typedef short         s16;
#define true          1
#define false         0
static u8 oldftoa(p8* s, u8 maxlen, u8 num_fraction, float32 fvalue)
{
    
        s32 digit, mnumber;
        s32 mnumber1 = 0;

        s32 t_char;
        u8 char_0 = 48;
        u8 out_loop = 0u;
        s8 fraction_count = -1;
        s16 index = 0u;

        s16 s_maxlen = maxlen;
        float32 f32_temp;
		float32 precision, weight;

        u8 negflag = 0;
        u8 useExpflag = 0;

        if (fvalue < 0)
        {
            negflag = 1;
            fvalue = -fvalue;
            s[index++] = '-';
        }
        // calculate magnitude
        f32_temp = (float)log10(fvalue);
        mnumber = (s32)f32_temp;


        //test that if we need the scientific notation or not
        if((negflag == 1 && (mnumber < -(s_maxlen - 3) || mnumber > (s_maxlen - 2)))
           ||(negflag == 0 && (mnumber < -(s_maxlen - 2) || mnumber > (s_maxlen - 1))))
        {
            useExpflag = 1;\
        }

        // set up for scientific notation
        if (useExpflag)
        {
            if (mnumber < 0)
            {
                mnumber -= 1;
            }
            f32_temp = (float32)mnumber;
            fvalue = fvalue / (float)pow(10.0f, f32_temp);
            mnumber1 = mnumber;
            mnumber = 0;
        }
        if (mnumber < 1)
        {
            mnumber = 0;
        }
        // convert the number
        f32_temp = (float32)s_maxlen;
        f32_temp = - f32_temp;
        precision = (float)pow(10.0f, f32_temp);

        while ((fvalue > precision || mnumber >= 0) && out_loop == 0u)
        {
            f32_temp = (float32)mnumber;
            weight = (float)pow(10.0f, f32_temp);

            if (weight > 0)
            {
                if(fraction_count >= 0)
                {
                    fraction_count++;
                    if(num_fraction < fraction_count)
                    {
                        out_loop = 1u; //break;
                    }
                }

                if(out_loop == 0u)
                {
                    digit = (s32)floor(fvalue / weight);
                    fvalue -= (digit * weight);
                    digit = digit > 9 ? 9: digit;
                    t_char = char_0 + digit;
                    s[index++] = (p8)t_char;

                    if(index >= s_maxlen)
                    {
                        out_loop = 1u; //break;
                    }
                }
            }
            if (mnumber == 0 && fvalue > precision && out_loop == 0u)
            {
                s[index++] = '.';
                fraction_count = 0;

                if(index >= s_maxlen)
                {
                    index--;
                    out_loop = 1u; //break;
                }
            }
            mnumber--;
        }
        if (useExpflag)
        {
            // convert the exponent
            p8 exponent[5];
            u8 exp_idx = 0;
            u8 exp_len, i;
            p8 exp_sign = '+';

            u8 fraction_len = (u8)index;

            if(mnumber1 < 0)
            {
                exp_sign = '-';
                mnumber1 = -mnumber1;
            }

            //reversed order
            while(mnumber1 > 0)
            {
                t_char = char_0 + mnumber1 % 10;
                exponent[exp_idx++] = (p8)t_char;
                mnumber1 /= 10;
            }

            exponent[exp_idx++] = exp_sign;
            exponent[exp_idx++] = 'e';

            exp_len = exp_idx;

            if(s_maxlen < (exp_len + fraction_len))
            {
                index = index - (exp_len - (s_maxlen - fraction_len));
            }

            //copy from exponent[] to s[] *reversed order*
            for(i = 0; i < exp_len; i++)
            {
                s[index++] = exponent[--exp_idx];
            }

        }

        s[index] = '\0';

        return (u8)index;
}
static void mn_round_off(p8 *s, u8 *pIndex, u8 dotIndex, u8 maxlen, u8 initialIndex)
{
	u8 j, k, index;
	u8 asc_0 = 0x30;
	index = *pIndex;
							for (j = 1; j <= (index - initialIndex); j++)
							{
								if ((index - j) == dotIndex)
								{
									continue;
								}
								if (9 == (u8)(s[index - j] - asc_0))
								{
									s[index - j] = (p8)asc_0;
								}
								else
								{
									s[index - j] = (p8)((u8)s[index - j] + 1);
									break;
								}
							}
							if (j == index + 1 - initialIndex)
							{
								for (k = index - 1; k > initialIndex; k--)
								{
									s[k] = s[k-1];
								}
								s[initialIndex] = (p8)(asc_0 + 1);
								if (index < maxlen)
								{
									s[index++] = (p8)asc_0;
								}
							}
							*pIndex = index;
							return;
}
static u8 ftoa(p8* s, u8 maxlen, u8 num_fraction, float32 fvalue)
{
    
        s8 mnumber;

        u8 asc_0 = 0x30;
        u8 index = 0, dotIndex = 1, i;
		u8 initialIndex = 0;

        float32 f32_temp;

        u8 negflag = 0;
        u8 useExpflag = 0;
		double temp;
		u8 utemp;

        if (fvalue < 0)
        {
            negflag = 1;
            fvalue = -fvalue;
            s[index++] = '-';
            dotIndex++;
			initialIndex = index;
        }

        // calculate magnitude
        f32_temp = (float)log10(fvalue);
		temp=floor((double)f32_temp);
        mnumber = (s8)floor((double)f32_temp);

        //test that if we need the scientific notation or not
        if((negflag == 1 && (mnumber <= -3 || mnumber >= 5))\
           ||(negflag == 0 && (mnumber <= -3 || mnumber >= 5)))
        {
            useExpflag = 1;
        }

        if (useExpflag == 1)
        {
            if (mnumber < 0)
            {
                mnumber = -mnumber;
                //caclulate the first bit
                fvalue = fvalue * (float)pow(10.0f, (float32)mnumber);
				utemp = (u8)(fvalue + 0.000001);
                s[index++] = (p8)(utemp + asc_0);
                s[index++] = '.';
                if (mnumber >= 10)
                {
                    for (i = 0; i < num_fraction; i++)
                    {
                        fvalue = (fvalue - (u8)fvalue)*10;
						utemp = (u8)(fvalue + 0.000001);
                        s[index++] = (p8)(utemp + asc_0);
                    }
                    //do the round-off
                    if ((u8)(fvalue+0.1) >= 5  && index > 0)
                    {
                        mn_round_off(s, &index, 1, maxlen, initialIndex);
                        index--;
                    }
					else
					{
						index--;
					}
                    s[index++] = '-';
                    s[index++] = 'e';
                    s[index++] = (p8)(mnumber/10 + asc_0);
                    s[index++] = (p8)(mnumber%10 + asc_0);
                }
                else
                {
                    for (i = 0; i <= num_fraction; i++)
                    {
                        fvalue = (fvalue - (u8)fvalue)*10;
						utemp = (u8)(fvalue + 0.000001);
                        s[index++] = (p8)(utemp + asc_0);
                    }
                    //do the round-off
                    if ((u8)(fvalue+0.1) >= 5  && index > 0)
                    {
                        mn_round_off(s, &index, 1, maxlen, initialIndex);
                        index--;
                    }
					else
					{
						index--;
					}
                    s[index++] = '-';
                    s[index++] = 'e';
                    s[index++] = (p8)(mnumber%10 + asc_0);
                }

            }
            else
            {
                //caclulate the first bit
                f32_temp = (float32)mnumber;
                fvalue = fvalue / (float)pow(10.0f, f32_temp);
				utemp = (u8)(fvalue + 0.000001);
                s[index++] = (p8)(utemp + asc_0);
                s[index++] = '.';
                if (mnumber >= 10)
                {
                    for (i = 0; i < num_fraction; i++)
                    {
                        fvalue = (fvalue - (u8)fvalue)*10;
						utemp = (u8)(fvalue + 0.000001);
                        s[index++] = (p8)(utemp + asc_0);
                    }
                    //do the round-off
                    if ((u8)(fvalue+0.1) >= 5  && index > 0)
                    {
                        mn_round_off(s, &index, 1, maxlen, initialIndex);
                        index--;
                    }
					else
					{
						index--;
					}
                    s[index++] = '+';
                    s[index++] = 'e';
                    s[index++] = (p8)(mnumber/10 + asc_0);
                    s[index++] = (p8)(mnumber%10 + asc_0);
                }
                else
                {
                    for (i = 0; i <= num_fraction; i++)
                    {
                        fvalue = (fvalue - (u8)(fvalue + 0.000001))*10;
						utemp = (u8)(fvalue + 0.000001);
                        s[index++] = (p8)(utemp + asc_0);
                    }

                    //do the round-off
                    if ((u8)(fvalue+0.1) >= 5  && index > 0)
                    {	
                        mn_round_off(s, &index, 1, maxlen, initialIndex);
                        index--;
                    }
					else
					{
						index--;
					}
                    s[index++] = '+';
                    s[index++] = 'e';
                    s[index++] = (p8)(mnumber%10 + asc_0);
                }
            }
        }
        else
        {
            //calculate the dot position
            while(fvalue >= 10.0)
            {
                dotIndex++;
                fvalue/=10;
            }
            for (index = initialIndex; index <= maxlen; index++)
            {
                //if the index is the dot index
                if (index == dotIndex)
                {
                    s[index] = '.';
                }
                else
                {
                    //the fraction number should not be exceded.
                    if (index <= (dotIndex + num_fraction) )
                    {
                        s[index] = (p8)(fvalue);
                        fvalue -= (u8)s[index];
                        fvalue *= 10;
                        utemp = (u8)((u8)s[index] + 0.000001);
                        s[index] = (p8)(utemp + asc_0);
                    }
                    //do the round-off
                    if (index == (dotIndex + num_fraction +1) )
                    {
                        if ((u8)(fvalue+0.1) >= 5 && index > 0)
                        {
                            mn_round_off(s, &index, dotIndex, maxlen, initialIndex);
                        }
						break;
                    }
                }
            }
        }

        s[index] = '\0';

        return (u8)index;
}

void main()   
  {   
          float   i,j;     
		  u8 len;
		  p8 buf[9];
		  for (j=0; j<100; j++)
          {
			  printf("enter   the   float   data:");   
          scanf("%f",&i);   
          len = ftoa(buf, 8, 2, i);
		  //len = oldftoa(buf, 8, 2, i);
          printf("%s\n",buf); 
		  printf("the lenght of the float is %u\n", len);
		  }
          return;   
  }