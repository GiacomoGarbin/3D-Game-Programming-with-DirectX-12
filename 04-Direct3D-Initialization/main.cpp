#include "ApplicationFramework.h"

class ApplicationInstance : public ApplicationFramework
{

};

int main()
{
	ApplicationInstance instance;

	if (!instance.init())
	{
		return 0;
	}

	return instance.run();
}