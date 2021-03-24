#include "ApplicationFramework.h"

class ApplicationInstance : public ApplicationFramework
{
public:
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;
};

void ApplicationInstance::update(GameTimer& timer)
{

}

void ApplicationInstance::draw(GameTimer& timer)
{

}

int main()
{
	ApplicationInstance instance;

	if (!instance.init())
	{
		return 0;
	}

	return instance.run();
}