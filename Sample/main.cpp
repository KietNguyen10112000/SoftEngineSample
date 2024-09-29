#include "Plugins/Bridge/PluginImpl.h"

#include "CharacterScript.h"
#include "ClimbingBar.h"

void RegisterSerializables()
{
	// registers all custom serializable classes, includes all scripts, all custom components like child classes of RenderingComponent,...
	// eg:
	//	SerializableDB::Get()->Register<MyScript1>();
	//	SerializableDB::Get()->Register<MyScript2>();
	//	...
	//	SerializableDB::Get()->Register<<custom serializable classes ClassName>>();

	SerializableDB::Get()->Register<CharacterScript>();
	SerializableDB::Get()->Register<ClimbingBar>();
}

void Initialize(Runtime* runtime)
{
	std::cout << "Hello, World from Sample!\n";
}

void Finalize(Runtime* runtime)
{

}