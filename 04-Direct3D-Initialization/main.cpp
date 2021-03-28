#include "ApplicationFramework.h"

class ApplicationInstance : public ApplicationFramework
{
	virtual void OnResize() override;
	virtual void update(GameTimer& timer) override;
	virtual void draw(GameTimer& timer) override;

public:
	ApplicationInstance();
	~ApplicationInstance();

	virtual bool init() override;
};

ApplicationInstance::ApplicationInstance() : ApplicationFramework()
{}

ApplicationInstance::~ApplicationInstance()
{}

bool ApplicationInstance::init()
{
	return ApplicationFramework::init();
}

void ApplicationInstance::OnResize()
{
	ApplicationFramework::OnResize();
}

void ApplicationInstance::update(GameTimer& timer)
{}

void ApplicationInstance::draw(GameTimer& timer)
{
	ThrowIfFailed(mCommandAllocator->Reset());

	//mCommandList->Reset(mCommandAllocator.Get(), nullptr);

	//ThrowIfFailed();
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