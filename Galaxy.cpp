#include "Galaxy.h"
#include <QStaticText>
#include <QFile>
#include <QJsonDocument>
#include <QDate>
#include <QGuiApplication>
#include <set>


QMap<QString, QColor> loadColors(const QString &fileName)
{
	QMap<QString, QColor> map = {{"Keller", Qt::transparent},
				     {"Pirates", Qt::white},
				     {"Terron", Qt::transparent},
				     {"Blazer", Qt::transparent},
				     {"Normals", QColor("orange")},
				     {"Maloc", QColor("red")},
				     {"Peleng", Qt::darkGreen},
				     {"People", QColor("royalblue")},
				     {"Fei", Qt::magenta},
				     {"Gaal", QColor("yellow")},

				     {"lineKeller", Qt::blue},
				     {"linePirates", QColor("grey")},
				     {"lineTerron", Qt::darkGreen},
				     {"lineBlazer", Qt::red},
				     {"lineNormals", QColor("orange")},
				     {"lineMaloc", QColor("red")},
				     {"linePeleng", Qt::darkGreen},
				     {"linePeople", QColor("royalblue")},
				     {"lineFei", Qt::magenta},
				     {"lineGaal", QColor("yellow")}};
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return map;
	}
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	if (doc.isNull()) {
		return map;
	}
	QVariantMap varMap = doc.toVariant().toMap();
	using VarMapCI = QVariantMap::const_iterator;
	for (VarMapCI i = varMap.begin(); i != varMap.end(); ++i) {
		map[i.key()] = QColor(i.value().toString());
	}
	return map;
}

const QMap<QString, QColor> Galaxy::_ownerToColor =
	loadColors("map_colors.json");

Galaxy::Galaxy()
{
}

void Galaxy::parseDump(QTextStream &stream)
{
	clear();
	const static QMap<QString, int> globalOptions = {
		{"Player ^{", 0}, {"StarList ^{", 1}, {"HoleList ^{", 2}};

	QString line;
	do {
		line = stream.readLine();
		switch (globalOptions.value(line, -1)) {
		case 0: // Player
		{
			Ship(stream, *this, 0, 0);
		} break;

		case 1: // StarList
			readStars(stream, *this);
			break;

		case 2: // HoleList
			readBlackHoles(stream, *this);
			break;
		case 3: // IDay
			break;

		default:
			if (line.startsWith("IDay=")) {
				currentDay = line.mid(5).toInt();
				std::cout << "currentDay=" << currentDay
					  << std::endl;
			}
			// skip record
			break;
		}
	} while (!line.isNull());
}

void Galaxy::clear()
{
	eqMap.clear();
	shipMap.clear();
	starMap.clear();
	planetMap.clear();
	blackHoles.clear();
	planetMarkets.clear();
	shipMarkets.clear();
	eqVec.clear();
	planetVec.clear();
	_minSellPrice.set(std::numeric_limits<unsigned>::max());
	_maxBuyPrice.set(0);
}

unsigned Galaxy::shipCount() const
{
	return shipMap.size();
}

unsigned Galaxy::equipmentCount() const
{
	return eqMap.size();
}

unsigned Galaxy::starCount() const
{
	return starMap.size();
}

unsigned Galaxy::blackHoleCount() const
{
	return blackHoles.size();
}

unsigned Galaxy::planetCount() const
{
	return planetMap.size();
}

unsigned Galaxy::galaxyTechLevel() const
{
	std::array<int, 9> ptlCount;
	ptlCount.fill(0);
	QStringList inhabited = {"PirateClan", "People", "Maloc",
				 "Fei",	       "Peleng", "Gaal"};
	for (const auto &pair : planetMap) {
		const Planet &p = pair.second;
		if (p.techLevel() >= ptlCount.size()) {
			return -1;
		}
		if (inhabited.contains(p.owner())) {
			++ptlCount[p.techLevel()];
		}
	}
	auto maxPtlIt = std::find_if(ptlCount.rbegin(), ptlCount.rend(),
				     [](const int &c) { return c > 0; });
	unsigned maxPtl = std::distance(maxPtlIt, ptlCount.rend());
	if (maxPtl < 1) {
		return 0;
	}
	if (ptlCount[maxPtl] >= 5) {
		return maxPtl;
	} else if (ptlCount[maxPtl] >= 2 || ptlCount[maxPtl - 1] >= 4) {
		return maxPtl - 1;
	}
	return maxPtl - 2;
}


void Galaxy::addEquipment(Equipment &&eq)
{
	assert(eqMap.find(eq.id())
	       == eqMap.end()); //"Galaxy: Tried to add an item which is already
				// existing. This should not happen!";
	eqVec.push_back(eq.id());
	eqMap.insert(std::make_pair(eq.id(), eq));
}

void Galaxy::addShip(const Ship &&ship)
{
	assert(shipMap.find(ship.id())
	       == shipMap.end()); //"Galaxy: Tried to add a ship which is
				  // already existing. This should not happen!"
	if (ship.hasMarket()) {
		shipMarkets.push_back(ship.id());
	}
	shipMap.insert(std::make_pair(ship.id(), ship));
}

void Galaxy::addStar(const Star &&star)
{
	assert(starMap.find(star.id())
	       == starMap.end()); //"Galaxy: Tried to add a star which is
				  // already existing. This should not happen!"
	galaxyMapRect |=
		QRectF(star.position().x(), star.position().y(), 1.0, 1.0);
	starMap.insert(std::move(std::make_pair(star.id(), std::move(star))));
}

void Galaxy::addBlackHole(const BlackHole &&bh)
{
	blackHoles.emplace_back(bh);
}

void Galaxy::addPlanet(const Planet &&planet)
{
	assert(planetMap.find(planet.id())
	       == planetMap
			  .end()); //"Galaxy: Tried to add a star which is
				   // already existing. This should not happen!"
	if (planet.hasMarket()) {
		planetMarkets.push_back(planet.id());
		_minSellPrice = _minSellPrice.min(planet.goodsSale(),
						  planet.goodsCount());
		_maxBuyPrice = _maxBuyPrice.max(planet.goodsBuy());
	}
	planetVec.push_back(planet.id());
	planetMap.insert(std::make_pair(planet.id(), planet));
}

QString Galaxy::starOwner(unsigned starId) const
{
	if (!starId) {
		return "";
	}
	const auto &star = starMap.at(starId);
	if (star.owner() == "Klings") {
		return star.domSeries();
	}
	return star.owner();
}


unsigned Galaxy::marketsCount() const
{
	return shipMarkets.size() + planetMarkets.size();
}

QString Galaxy::marketName(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).name();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets))
			.name();
	}
	return ""; // should never reach hear
}

QString Galaxy::marketPlanetEconomy(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();

	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).economy();
	}
	return "";
}

QString Galaxy::marketPlanetOwner(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();

	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).owner();
	}
	return "";
}

unsigned Galaxy::marketPlanetSize(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();

	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).size();
	}
	return 0;
}

unsigned Galaxy::marketPlanetTechLevel(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();

	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).techLevel();
	}
	return 0;
}

const GoodsArr &Galaxy::marketQuantity(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).goodsCount();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets))
			.goodsCount();
	}
	// return GoodsArr;//should never reach hear
}

const GoodsArr &Galaxy::marketSale(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).goodsSale();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets))
			.goodsSale();
	}
	// return GoodsArr;//should never reach hear
}

const GoodsArr &Galaxy::marketBuy(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).goodsBuy();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets))
			.goodsBuy();
	}
	// return GoodsArr;//should never reach hear
}

unsigned Galaxy::marketId(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).id();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets)).id();
	}
	return -1; // should never reach hear
}

double Galaxy::marketDistFromPlayer(unsigned row) const
{
	assert(shipMap.count(0));
	unsigned mStarId = marketStarId(row);
	unsigned playerStarId = shipMap.at(0).starId();
	QPointF playerPos = starMap.at(playerStarId).position();
	QPointF marketPos = starMap.at(mStarId).position();
	QPointF delta = playerPos - marketPos;
	return sqrt(pow(delta.x(), 2) + pow(delta.y(), 2));
}

QString Galaxy::marketStarName(unsigned row) const
{
	unsigned starId = marketStarId(row);
	return starMap.at(starId).name();
}

unsigned Galaxy::equipmentId(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).id();
}

QString Galaxy::equipmentName(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).name();
}

QString Galaxy::equipmentType(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).type();
}

unsigned Galaxy::equipmentSize(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).size();
}

QString Galaxy::equipmentOwner(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).owner();
}

unsigned Galaxy::equipmentCost(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).cost();
}

unsigned Galaxy::equipmentTechLevel(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).techLevel();
}

QString Galaxy::equipmentLocationType(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).locationTypeString();
}

QString Galaxy::equipmentLocationName(unsigned row) const
{
	const Equipment &eq = eqMap.at(eqVec.at(row));
	Equipment::LocationType locType = eq.locationType();
	unsigned locId = eq.locationId();
	switch (locType) {
	case Equipment::kShipEq:
	case Equipment::kShipStorage:
	case Equipment::kShipShop:
		return shipMap.at(locId).name();
	case Equipment::kJunk:
		return starMap.at(locId).name();
	case Equipment::kPlanetShop:
	case Equipment::kPlanetStorage:
	case Equipment::kPlanetTreasure:
		return planetMap.at(locId).name();
	}
	return "";
}

int Galaxy::equipmentDepth(unsigned row) const
{
	const Equipment &eq = eqMap.at(eqVec.at(row));
	Equipment::LocationType locType = eq.locationType();
	if (locType == Equipment::kPlanetTreasure) {
		return eq.extra("Depth").toInt();
	}
	return -1;
}

QString Galaxy::equipmentStarName(unsigned row) const
{
	int starId = equipmentStarId(row);
	return starId ? starMap.at(starId).name() : "Tranclucator";
	/*Equipment::LocationType
    locType=eqMap.at(eqVec.at(row)).locationType(); unsigned
    locId=eqMap.at(eqVec.at(row)).locationId(); switch (locType)
    {
    case Equipment::kShipEq:
    case Equipment::kShipStorage:
    case Equipment::kShipShop:
    {
	unsigned starId=shipMap.at(locId).starId();
	if(starId){
	    return starMap.at(starId).name();
	}
	else {//tranclucator
	    return "(Tranclucator)";
	}
    }
    break;

    case Equipment::kJunk:
	return starMap.at(locId).name();
    case Equipment::kPlanetShop:
    case Equipment::kPlanetStorage:
    case Equipment::kPlanetTreasure:
    {
	unsigned starId=planetMap.at(locId).starId();
	if(starId){
	   return starMap.at(starId).name();
	}
    }
    break;
    }
    return "...";*/
}

double Galaxy::equipmentDistFromPlayer(unsigned row) const
{
	unsigned eqStarId = equipmentStarId(row);
	if (!eqStarId) {
		// return std::numeric_limits<double>::quiet_NaN();
		return std::numeric_limits<double>::infinity();
	}
	unsigned playerStarId = shipMap.at(0).starId();
	QPointF playerPos = starMap.at(playerStarId).position();
	QPointF eqPos = starMap.at(eqStarId).position();
	QPointF delta = playerPos - eqPos;
	return sqrt(pow(delta.x(), 2) + pow(delta.y(), 2));
}

QString Galaxy::equipmentStarOwner(unsigned row) const
{
	unsigned eqStarId = equipmentStarId(row);
	return starOwner(eqStarId);
}

double Galaxy::equipmentDurability(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).durability();
}

QString Galaxy::equipmentBonus(unsigned row) const
{
	return eqMap.at(eqVec.at(row)).bonusNote();
}

unsigned Galaxy::blackHoleId(unsigned row) const
{
	return blackHoles[row].id();
}

QString Galaxy::blackHoleStar1(unsigned row) const
{
	unsigned starId = blackHoles[row].star1Id();
	return starMap.at(starId).name();
}

float Galaxy::blackHoleStar1Distance(unsigned row) const
{
	unsigned bhStarId = blackHoles[row].star1Id();
	unsigned playerStarId = shipMap.at(0).starId();
	QPointF playerPos = starMap.at(playerStarId).position();
	QPointF eqPos = starMap.at(bhStarId).position();
	QPointF delta = playerPos - eqPos;
	return sqrt(pow(delta.x(), 2) + pow(delta.y(), 2));
}

QString Galaxy::blackHoleStar2(unsigned row) const
{
	unsigned starId = blackHoles[row].star2Id();
	return starMap.at(starId).name();
}

float Galaxy::blackHoleStar2Distance(unsigned row) const
{
	unsigned bhStarId = blackHoles[row].star2Id();
	unsigned playerStarId = shipMap.at(0).starId();
	QPointF playerPos = starMap.at(playerStarId).position();
	QPointF eqPos = starMap.at(bhStarId).position();
	QPointF delta = playerPos - eqPos;
	return sqrt(pow(delta.x(), 2) + pow(delta.y(), 2));
}

int Galaxy::blackHoleTurnsToClose(unsigned row) const
{
	return blackHoles[row].turnsToClose();
}

QString Galaxy::blackHoleNextLootChange(unsigned row) const
{
	QDate today = QDate(3300, 1, 1).addDays(currentDay - 301);
	QString changes;
	int ttclose = blackHoleTurnsToClose(row);
	int lastChange = std::min(ttclose, 77 * 5);
	lastChange = ttclose < 1 ? 77 * 5 : lastChange;
	for (unsigned daysToChange = 77 - (currentDay % 77);
	     daysToChange < lastChange; daysToChange += 77) {
		changes += today.addDays(daysToChange).toString("dd MMMM yyyy")
			   + "; ";
	}
	if (lastChange < ttclose) {
		changes += " ...";
	}
	return changes;
}

struct NumShips {
	int normals = 0;
	int pirates = 0;
	int kellers = 0;
	int terrons = 0;
	int blazers = 0;
	const QString dummy = "<font color=%2>%1<color>/";
	QString infoStr(const QMap<QString, QColor> &colMap)
	{
		QString str;
		if (normals > 0)
			str += dummy.arg(normals).arg(
				colMap["lineNormals"].name());
		if (pirates > 0)
			str += dummy.arg(pirates).arg(
				colMap["linePirates"].name());
		if (kellers > 0)
			str += dummy.arg(kellers).arg(
				colMap["lineKeller"].name());
		if (terrons > 0)
			str += dummy.arg(terrons).arg(
				colMap["lineTerron"].name());
		if (blazers > 0)
			str += dummy.arg(blazers).arg(
				colMap["lineBlazer"].name());
		if (!str.isEmpty())
			str[str.length() - 1] = ' ';
		return str;
	}
};

QImage Galaxy::map(const unsigned width, const int fontSize) const
{
	// QImage
	// image((mapRect.width()+8)*scale,(mapRect.height()+6)*scale,QImage::Format_ARGB32);
	const unsigned padding = fontSize * 4;
	const unsigned netWidth = width - 2.0 * padding;
	const unsigned height =
		2.0 * padding
		+ netWidth * galaxyMapRect.height() / galaxyMapRect.width();
	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::black);
	QPainter p(&image);
	p.setPen(QPen(QColor(Qt::white)));
	p.setBrush(QBrush(QColor(Qt::white), Qt::SolidPattern));
	p.setRenderHint(QPainter::Antialiasing, true);
	QFont font("Arial");
	//        font.setStretch(QFont::SemiCondensed);
	font.setPointSize(fontSize);
	p.setFont(font);
	// prepare base names
	QMap<unsigned, QString> starIdToBases;
	for (unsigned baseId : shipMarkets) {
		const Ship &ship = shipMap.at(baseId);
		QString base = ship.name().left(2);
		unsigned starId = ship.starId();
		QString &basesStr = starIdToBases[starId];
		if (basesStr.length()) {
			basesStr += ',';
		}
		basesStr += base;
	}
	// prepare planets
	QMap<unsigned, QString> starIdToPlanets;
	const QString planetTemplate("<font color=%3>%1%2<color>");
	for (const auto &pair : planetMap) {
		const Planet &planet = pair.second;
		if (planet.owner() == "None") {
			continue;
		}
		unsigned starId = planet.starId();
		QString &planetsStr = starIdToPlanets[starId];
		QString economy = planet.economy().left(1).toLower();
		int size = planet.size();
		QString color = _ownerToColor.value(planet.race()).name();
		planetsStr += planetTemplate.arg(size).arg(economy).arg(color);
	}
	std::set<unsigned> bhStarIds;
	for (const BlackHole &bh : blackHoles) {
		bhStarIds.insert(bh.star1Id());
		bhStarIds.insert(bh.star2Id());
	}

	std::map<unsigned, NumShips> starShips;
	for (const auto &pair : shipMap) {
		const Ship &ship = pair.second;
		const unsigned starid = ship.starId();
		const QString race = ship.race();
		if (race == "Normal")
			++starShips[starid].normals;
		else if (race == "Pirate")
			++starShips[starid].pirates;
		else if (race == "Keller")
			++starShips[starid].kellers;
		else if (race == "Terron")
			++starShips[starid].terrons;
		else if (race == "Blazer")
			++starShips[starid].blazers;
		else
			std::cerr << "ERROR! Unexpeced ship race: "
					     + race.toStdString()
				  << std::endl;
	}

	// Draw stars
	const double scale = double(netWidth) / galaxyMapRect.width();
	const double starR = 0.5 * fontSize;
	const double starLineW = 0.5 * starR;

	// QMap<unsigned,QPointF> starIdToPos;
	for (const auto &pair : starMap) {
		const Star &star = pair.second;
		QPointF pos = star.position() - galaxyMapRect.topLeft();
		pos *= scale;
		pos += QPointF(padding, padding);
		// starIdtoPos[star.id()]=pos;
		QString owner = star.owner();
		if (owner == "Klings") {
			owner = star.domSeries();
		}
		p.setPen(Qt::white);
		p.setBrush(
			QBrush(QColor(_ownerToColor[owner]), Qt::SolidPattern));
		QRectF nameRect;
		nameRect.setTopLeft(pos + QPointF(-6 * fontSize, starR));
		nameRect.setBottomRight(
			pos + QPointF(fontSize * 6, fontSize * 2 + starR));
		// p.drawRect(nameRect);
		p.drawText(nameRect, star.name(),
			   QTextOption(Qt::AlignHCenter | Qt::AlignTop));

		const QString &basesStr = starIdToBases.value(star.id());
		if (!basesStr.isEmpty()) {
			QRectF basesRect =
				QRectF(pos + QPointF(-8.0 * fontSize, -starR),
				       pos + QPointF(-1.1 * starR, starR));
			p.drawText(
				basesRect, basesStr.left(2),
				QTextOption(Qt::AlignRight | Qt::AlignVCenter));
			if (basesStr.length() > 3) {
				basesRect.translate(
					8.0 * fontSize + 1.2 * starR, 0);
				p.drawText(basesRect, basesStr.mid(3),
					   QTextOption(Qt::AlignLeft
						       | Qt::AlignVCenter));
			}
		}
		p.setPen(
			QPen(QBrush(_ownerToColor["line" + owner]), starLineW));
		p.drawEllipse(pos, starR, starR);
		QString shipsInfo = starShips[star.id()].infoStr(_ownerToColor);
		QStaticText planetsText(shipsInfo
					+ starIdToPlanets.value(star.id()));
		planetsText.setTextFormat(Qt::RichText);
		planetsText.prepare(QTransform(), font);
		QPointF topLeft(pos);
		topLeft.rx() -= 0.5 * planetsText.size().width();
		topLeft.ry() -= 1.5 * fontSize + starR;
		p.drawStaticText(topLeft, planetsText);

		if (bhStarIds.count(star.id())) {
			p.setBrush(
				QBrush(QColor("darkblue"), Qt::SolidPattern));
			p.setPen(QPen(QBrush(QColor("deepskyblue")),
				      starR * 0.4));
			p.drawEllipse(pos + QPointF(starR * 0.8, starR * 0.8),
				      starR * 0.7, starR * 0.7);
		}
	}
	return image;
}

unsigned Galaxy::marketStarId(unsigned row) const
{
	unsigned numPlanetMarkets = planetMarkets.size();
	assert(row < marketsCount());
	if (row < numPlanetMarkets) {
		return planetMap.at(planetMarkets.at(row)).starId();
	} else {
		return shipMap.at(shipMarkets.at(row - numPlanetMarkets))
			.starId();
	}
}

unsigned Galaxy::equipmentStarId(unsigned row) const
{
	Equipment::LocationType locType =
		eqMap.at(eqVec.at(row)).locationType();
	unsigned locId = eqMap.at(eqVec.at(row)).locationId();
	switch (locType) {
	case Equipment::kShipEq:
	case Equipment::kShipStorage:
	case Equipment::kShipShop: {
		return shipMap.at(locId).starId();
	} break;

	case Equipment::kJunk:
		return locId;
	case Equipment::kPlanetShop:
	case Equipment::kPlanetStorage:
	case Equipment::kPlanetTreasure: {
		return planetMap.at(locId).starId();
	} break;
	}
	return 0;
}
