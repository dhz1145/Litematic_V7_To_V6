#include "MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral("Litematic_V7_To_V6_GUI"));
	QApplication::setOrganizationName(QStringLiteral("LitematicTools"));

	// Minecraft grass block window / taskbar icon
	const QIcon appIcon(QStringLiteral(":/icons/grass_block.png"));
	QApplication::setWindowIcon(appIcon);

	MainWindow window;
	window.setWindowIcon(appIcon);
	window.show();

	return app.exec();
}
