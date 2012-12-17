extern void *_Znwj(unsigned int);

void *_Znwm(unsigned long sz)
{
	return _Znwj(sz);
}
