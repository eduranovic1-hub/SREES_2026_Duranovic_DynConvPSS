#include "PSSPlugin.h"
#include <sc/IPlugin.h>
#include "WindowPlugin.h"
#include <td/StringUtils.h>
#include <dense/Matrix.h>
#include <mu/ScopedCLocale.h>
#include <xml/DOMParser.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>

// ============================================================
// Plugin klasa - standardna natID struktura, ne dirati
// ============================================================
class Plugin : public sc::IPlugin
{
    MemoryArchiveContainer _outArchives;
    WindowPlugin* _pWnd = nullptr;
public:
    Plugin()
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = nullptr;
    }

    void show(gui::Window* parentWnd, MemoryArchiveContainer& archives, td::UINT4 wndID, const sc::IPlugin::Cleaner& cleaner, const sc::IPlugin::CallBack& onComplete) override final
    {
        for (size_t i = 0; i < size_t(ArchType::NA); ++i)
            _outArchives[i] = archives[i];

        if (_pWnd)
            _pWnd->setFocus();
        else
        {
            _pWnd = new WindowPlugin(parentWnd, this, onComplete, cleaner, wndID);
            _pWnd->open();
        }
    }

    td::String getMenuName() const override final
    {
        return "PSS Converter";
    }

    arch::MemoryOut* getArchive(sc::IPlugin::ArchType type) override final
    {
        auto iType = size_t(type);
        if (iType >= getMaxSupportedArchiveParts())
            return nullptr;
        return _outArchives[size_t(type)];
    }

    MemoryArchiveContainer& getArchives() override final
    {
        return _outArchives;
    }

    td::String getOutFileName() const override final
    {
        assert(_pWnd);
        return _pWnd->getOutFileName();
    }

    size_t getMaxSupportedArchiveParts() const override final
    {
        return size_t(ArchType::NA);
    }

    ModelType getModelType() const override final
    {
        return ModelType::DAE;
    }

    void onClosedPluginWindow()
    {
        _pWnd = nullptr;
    }
};

static Plugin s_plugin;

void onClosedPluginWindow()
{
    s_plugin.onClosedPluginWindow();
}

extern "C"
{
    PLUGIN_API sc::IPlugin* getPluginInterface()
    {
        return &s_plugin;
    }
}

// ============================================================
// IEEE Case 9 - linijski podaci (R, X, B) - standardni podaci
// Sabirnice: 1=slack(gen), 2=gen, 3=gen, 4-9=mreza/potrosaci
// ============================================================
struct LineData { int from, to; double r, x, b; };

static LineData case9Lines[] = {
    {1, 4, 0.0,   0.0576, 0.0},
    {4, 5, 0.010, 0.0850, 0.176},
    {5, 6, 0.017, 0.0920, 0.158},
    {3, 6, 0.0,   0.0586, 0.0},
    {6, 7, 0.0119,0.1008, 0.209},
    {7, 8, 0.0085,0.0720, 0.149},
    {8, 2, 0.0,   0.0625, 0.0},
    {8, 9, 0.0320,0.1610, 0.306},
    {9, 4, 0.0390,0.1700, 0.358}
};
static const int case9LineCount = 9;
static const int case9BusCount = 9;

struct BusInit { int id; double pLoad, qLoad, pGen, vSet; bool isSlack; bool isPV; };

// Slack = bus 1, PV (generator) = bus 2 i 3, ostali su PQ potrosaci/cvorovi mreze
static BusInit case9Buses[] = {
    {1, 0.0,   0.0,   0.0,   1.040, true,  false}, // slack
    {2, 0.0,   0.0,   1.63,  1.025, false, true},  // PV - generator 2
    {3, 0.0,   0.0,   0.85,  1.025, false, true},  // PV - generator 3
    {4, 0.0,   0.0,   0.0,   1.0,   false, false},
    {5, 1.25,  0.50,  0.0,   1.0,   false, false},
    {6, 0.90,  0.30,  0.0,   1.0,   false, false},
    {7, 0.0,   0.0,   0.0,   1.0,   false, false},
    {8, 1.00,  0.35,  0.0,   1.0,   false, false},
    {9, 0.0,   0.0,   0.0,   1.0,   false, false}
};
static const int case9GenBuses[] = { 2, 3 };
static const int case9GenCount = 2;

// ============================================================
// CSV reader za Case 30/118/300 - generlcki ucitava bus i line
// podatke iz CSV fajlova (radi za bilo koju velicinu mreze).
// Format buses.csv: id,type,pLoad,qLoad,pGen,vSet  (type: 1=PQ,2=PV,3=slack)
// Format lines.csv: from,to,r,x,b
// ============================================================
static bool readBusesCsv(const td::String& fileName, cnt::PushBackVector<BusInit>& outBuses, cnt::PushBackVector<int>& outGenBuses)
{
    fo::InFile inFile;
    if (!fo::openExistingBinaryFile(inFile, fileName))
        return false;

    fo::LineNormal buffer;
    bool firstLine = true;
    while (fo::getLine(inFile, buffer))
    {
        if (firstLine) { firstLine = false; continue; } // skip header

        td::String line(buffer.c_str());
        if (line.isEmpty())
            continue;

        cnt::PushBackVector<td::String> tok;
        tok.reserve(8);
        line.split(",", tok);
        if (tok.size() < 6)
            continue;

        BusInit b;
        b.id = std::atoi(tok[0].c_str());
        int typ = std::atoi(tok[1].c_str());
        b.pLoad = std::atof(tok[2].c_str());
        b.qLoad = std::atof(tok[3].c_str());
        b.pGen = std::atof(tok[4].c_str());
        b.vSet = std::atof(tok[5].c_str());
        b.isSlack = (typ == 3);
        b.isPV = (typ == 2);

        outBuses.push_back(b);
        if (b.isPV || b.isSlack)
            if (!b.isSlack) // slack se posebno tretira (uvijek prvi/referentni)
                outGenBuses.push_back(b.id);
    }
    return outBuses.size() > 0;
}

static bool readLinesCsv(const td::String& fileName, cnt::PushBackVector<LineData>& outLines)
{
    fo::InFile inFile;
    if (!fo::openExistingBinaryFile(inFile, fileName))
        return false;

    fo::LineNormal buffer;
    bool firstLine = true;
    while (fo::getLine(inFile, buffer))
    {
        if (firstLine) { firstLine = false; continue; }

        td::String line(buffer.c_str());
        if (line.isEmpty())
            continue;

        cnt::PushBackVector<td::String> tok;
        tok.reserve(8);
        line.split(",", tok);
        if (tok.size() < 5)
            continue;

        LineData l;
        l.from = std::atoi(tok[0].c_str());
        l.to = std::atoi(tok[1].c_str());
        l.r = std::atof(tok[2].c_str());
        l.x = std::atof(tok[3].c_str());
        l.b = std::atof(tok[4].c_str());
        outLines.push_back(l);
    }
    return outLines.size() > 0;
}

static int findBusIndex(BusInit* buses, int nBus, int busId)
{
    for (int i = 0; i < nBus; ++i)
        if (buses[i].id == busId)
            return i;
    return 0; // fallback - ne bi se trebalo desiti za validne podatke
}

// ============================================================
// Parsiranje mape "generator:tip" npr. "2:1,3:3"
// 0=Standard, 1=Type I (Δω), 2=Type II (Pe), 3=Type III+AVR
// ============================================================
static int getPssTypeForGenerator(const td::String& genPssMap, int genId)
{
    if (genPssMap.isEmpty())
        return 0;

    cnt::PushBackVector<td::String> pairs;
    pairs.reserve(16);
    genPssMap.split(",", pairs);

    auto n = pairs.size();
    for (td::UINT4 i = 0; i < n; ++i)
    {
        cnt::PushBackVector<td::String> kv;
        kv.reserve(2);
        pairs[i].split(":", kv);
        if (kv.size() != 2)
            continue;
        if (std::atoi(kv[0].c_str()) == genId)
            return std::atoi(kv[1].c_str());
    }
    return 0;
}

// ============================================================
// Citanje konfiguracije generator:tip iz XML fajla (natID xml::FileParser)
// Format XML-a:
//   <PSSConfig>
//       <Generator id="2" type="1"/>   <!-- 1=Type I, 2=Type II, 3=Type III+AVR -->
//       <Generator id="3" type="3"/>
//   </PSSConfig>
// Vraca td::String mapu u formatu "id1:tip1,id2:tip2,..." (interni format
// koji ostatak koda vec koristi), ili praznu ako XML nije validan/ne postoji.
// ============================================================
static td::String readPssConfigFromXml(const td::String& xmlFileName, gui::LineEdit& status)
{
    if (xmlFileName.isEmpty() || !fo::fileExists(xmlFileName))
        return td::String(); // koristi standardni model ako XML nije naveden

    xml::FileParser parser;
    if (!parser.parseFile(xmlFileName))
    {
        status = "WARNING! Cannot parse XML config, using standard model for all generators.";
        return td::String();
    }

    auto root = parser.getRootNode(); // <PSSConfig>
    if (!root.isOk())
        return td::String();

    td::MutableString result;
    result.reserve(256);
    bool first = true;

    auto genNode = root.getChildNode("Generator");
    while (genNode.isOk())
    {
        td::INT4 genId = 0;
        td::INT4 pssType = 0;
        genNode.getAttribValue("id", genId);
        genNode.getAttribValue("type", pssType);

        if (!first) result.append(",");
        first = false;
        result.appendFormat("%d:%d", int(genId), int(pssType));

        ++genNode;
    }

    return td::String(result.c_str());
}

// ============================================================
// Sastavljanje Ybus matrice koristeci natID dense::DblMatrix
// Ybus se sastoji od dvije NxN matrice: G (konduktansa) i B (susceptansa)
// Ovo je OBAVEZAN dio projekta - koristenje natID dense matrica
// za sve mrezne proracune (admitansna matrica sistema)
// ============================================================
struct YbusMatrices
{
    dense::DblMatrix G; // realni dio admitansne matrice
    dense::DblMatrix B; // imaginarni dio admitansne matrice
};

static YbusMatrices buildYbus(int nBus, LineData* lines, int nLines)
{
    YbusMatrices ybus;
    ybus.G.reserve(td::UINT4(nBus), td::UINT4(nBus), nullptr, true); // initZeros=true
    ybus.B.reserve(td::UINT4(nBus), td::UINT4(nBus), nullptr, true);

    auto matG = ybus.G.getManipulator1(); // 1-based indeksiranje (id sabirnice = indeks)
    auto matB = ybus.B.getManipulator1();

    for (int l = 0; l < nLines; ++l)
    {
        int f = lines[l].from;
        int t = lines[l].to;
        double r = lines[l].r;
        double x = lines[l].x;
        double bsh = lines[l].b;

        double denom = r * r + x * x;
        double g = (denom > 0.0) ? (r / denom) : 0.0;
        double b = (denom > 0.0) ? (-x / denom) : 0.0;

        // van-dijagonalni elementi (negativna admitansa veze)
        matG(f, t) -= g;  matG(t, f) -= g;
        matB(f, t) -= b;  matB(t, f) -= b;

        // dijagonalni elementi (zbir svih admitansi povezanih na sabirnicu + shunt/2)
        matG(f, f) += g;  matG(t, t) += g;
        matB(f, f) += b + bsh / 2.0;
        matB(t, t) += b + bsh / 2.0;
    }

    return ybus;
}

// ============================================================
// Glavna createModel funkcija
// Thread 1: racuna i pise .dmodl/.vmodl fajlove (konverzija)
// Thread 2: simulira progres indikator nezavisno
// ============================================================
bool createModel(const td::String& inputFileName, const td::String& outFileName, sc::IPlugin* pIPlugin, const Options& options, gui::LineEdit& status)
{
    mu::ScopedCLocale scopedLocale;

    std::atomic<int> progress(0);
    std::atomic<bool> done(false);

    std::thread progressThread([&progress, &done]()
        {
            while (!done.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });

    bool success = false;

    std::thread convThread([&]()
        {
            progress = 10;

            // -- Citanje PSS konfiguracije iz XML fajla (In File Name iz GUI-a) --
            // Ako XML nije validan/ne postoji, koristi se mapa iz Options taba kao rezerva.
            td::String genPssMap = readPssConfigFromXml(inputFileName, status);
            if (genPssMap.isEmpty())
                genPssMap = options.genPssMap; // rezervna opcija (manuelni unos u GUI)

            // -- Odabir mreze prema case broju --
            // Case 9: ugradjeni podaci (default/fallback). Case 30/118/300: ucitava se
            // iz CSV fajlova (case<N>_buses.csv, case<N>_lines.csv) u istom folderu
            // kao i izlazni .dmodl fajl. Ovo je genericki pristup koji radi za bilo
            // koju velicinu mreze bez potrebe za hardkodiranjem u C++.
            cnt::PushBackVector<BusInit> csvBuses;
            cnt::PushBackVector<int> csvGenBuses;
            cnt::PushBackVector<LineData> csvLines;

            int nBus = case9BusCount;
            BusInit* buses = case9Buses;
            LineData* lines = case9Lines;
            int nLines = case9LineCount;
            const int* genBuses = case9GenBuses;
            int genCount = case9GenCount;

            if (options.caseNumber == 30 || options.caseNumber == 118 || options.caseNumber == 300)
            {
                // Izvuci folder iz putanje outFileName (rucno, bez ovisnosti o fo::getPath)
                td::String outDir;
                {
                    const char* pStr = outFileName.c_str();
                    const char* pLastSlash = nullptr;
                    for (const char* p = pStr; *p; ++p)
                    {
                        if (*p == '\\' || *p == '/')
                            pLastSlash = p;
                    }
                    if (pLastSlash)
                        outDir = td::String(pStr, td::UINT4(pLastSlash - pStr));
                    else
                        outDir = td::String(".");
                }
                td::MutableString busFile, lineFile;
                busFile.appendFormat("%s/case%d_buses.csv", outDir.c_str(), int(options.caseNumber));
                lineFile.appendFormat("%s/case%d_lines.csv", outDir.c_str(), int(options.caseNumber));

                bool okB = readBusesCsv(td::String(busFile.c_str()), csvBuses, csvGenBuses);
                bool okL = readLinesCsv(td::String(lineFile.c_str()), csvLines);

                // Kopiraj iz natID PushBackVector u obican niz (siguran kontinuirani memorijski layout
                // za pointer aritmetiku koju koristi ostatak koda - PushBackVector ne garantuje to).
                static BusInit* pBusArr = nullptr;
                static LineData* pLineArr = nullptr;
                static int* pGenArr = nullptr;

                if (okB && okL)
                {
                    nBus = int(csvBuses.size());
                    pBusArr = new BusInit[nBus];
                    for (int i = 0; i < nBus; ++i) pBusArr[i] = csvBuses[i];
                    buses = pBusArr;

                    nLines = int(csvLines.size());
                    pLineArr = new LineData[nLines];
                    for (int i = 0; i < nLines; ++i) pLineArr[i] = csvLines[i];
                    lines = pLineArr;

                    genCount = int(csvGenBuses.size());
                    pGenArr = new int[genCount];
                    for (int i = 0; i < genCount; ++i) pGenArr[i] = csvGenBuses[i];
                    genBuses = pGenArr;
                }
                else
                {
                    status = "WARNING! CSV files for case ";
                    // (fallback ostaje na case9 podacima postavljenim iznad)
                }
            }

            // -- Sastavi Ybus matricu koristeci natID dense::DblMatrix --
            YbusMatrices ybus = buildYbus(nBus, lines, nLines);
            auto Gm = ybus.G.getManipulator1();
            auto Bm = ybus.B.getManipulator1();

            progress = 25;


            std::ofstream fOut;
            if (!fo::createTextFile(fOut, outFileName))
            {
                status = "ERROR! Cannot create output file!";
                success = false; done = true; return;
            }

            // -------------------- HEADER --------------------
            fOut << "Header:\n";
            fOut << "\tmaxIter = " << options.maxIter << "\n";
            fOut << "\treport = Solver\n";
            fOut << "\tmaxReps = -1\n";
            fOut << "\toutToTxt = false\n";
            fOut << "\ttxtFile = \"\"\n";
            fOut << "\tstartTime = 0\n";
            fOut << "\tdTime = " << options.dTime << "\n";
            fOut << "\tendTime = " << options.endTime << "\n";
            fOut << "end\n\n";

            fOut << "// PSS Dynamic Converter Model - IEEE Case " << options.caseNumber << "\n";
            fOut << "// Generated by PSS Plugin Converter (Power Flow + PSS Type I/II/III + AVR)\n\n";

            td::String modelName = options.modelName;
            modelName.replace('\"', '\'');

            // -------------------- MAIN MODEL --------------------
            fOut << "Model [type=DAE domain=real method=RK2 name=\"" << modelName.c_str() << "\" eps=1e-8]:\n\n";

            progress = 35;

            // -- Izracunaj realisticnu inicijalnu Eqp vrijednost za svaki generator --
            // Standardna formula: Eq' = sqrt((V + Xd'*Q/V)^2 + (Xd'*P/V)^2)
            // Ovo sprijecava da DAE solver pocne iz neravnotezne tacke (sto uzrokuje
            // numericku nestabilnost / divergenciju omega i delta).
            const double XdpVal = 0.3; // mora odgovarati Xdp parametru koriscenom dolje
            cnt::PushBackVector<double> eqpInit;
            eqpInit.reserve(genCount > 0 ? genCount : 1);
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                int idx = -1;
                for (int i = 0; i < nBus; ++i) if (buses[i].id == genId) idx = i;
                double P = buses[idx].pGen;
                double V = buses[idx].vSet;
                double Q = 0.3; // procjena reaktivne snage generatora (poboljsava inicijalizaciju)
                double term1 = V + (XdpVal * Q) / V;
                double term2 = (XdpVal * P) / V;
                eqpInit.push_back(std::sqrt(term1 * term1 + term2 * term2));
            }

            // ---- Vars: dinamicka stanja generatora + mrezni naponi (vr,vi) ----
            fOut << "Vars [out=true]:\n";
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                int pssType = getPssTypeForGenerator(genPssMap, genId);
                fOut << "\tdelta" << genId << " = 0.0\n";
                fOut << "\tomega" << genId << " = 1.0\n";
                fOut << "\tEqp" << genId << " = " << eqpInit[g] << "\n";

                if (pssType == 0) // Standardni model: konstantna pobuda (bez AVR/PSS)
                {
                    // nema dodatnih dinamickih stanja
                }
                else if (pssType == 1) // PSS Type I: Δω signal, wash-out + lead-lag, dodano na konstantnu pobudu
                {
                    fOut << "\txw" << genId << " = 0.0\n";
                    fOut << "\tEfd" << genId << " = " << eqpInit[g] << "\n";
                    fOut << "\tVm" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
                else if (pssType == 2) // PSS Type II: Pe signal, integrator + wash-out + lead-lag
                {
                    fOut << "\txw" << genId << " = 0.0\n";
                    fOut << "\txi" << genId << " = 0.0\n";
                    fOut << "\tEfd" << genId << " = " << eqpInit[g] << "\n";
                    fOut << "\tVm" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
                else if (pssType == 3) // PSS Type III + AVR: dvokanalni (Δω i Pe) + AVR dinamika
                {
                    fOut << "\txw" << genId << " = 0.0\n";
                    fOut << "\tEfd" << genId << " = " << eqpInit[g] << "\n";
                    fOut << "\tVm" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
            }

            // mrezni naponi (real/imag) za sve sabirnice
            for (int i = 0; i < nBus; ++i)
            {
                if (!buses[i].isSlack)
                    fOut << "\tvr" << buses[i].id << " = " << buses[i].vSet << "; vi" << buses[i].id << " = 0.0\n";
            }
            fOut << "\n";

            progress = 45;

            // ---- Params: mrezni i dinamicki parametri ----
            fOut << "Params:\n";

            // pomocne algebarske velicine (izvedene - idu u Params, ne u Vars)
            fOut << "\t// -- Pomocne velicine (izvedene preko PostProc) --\n";
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                fOut << "\tPe" << genId << "; Vmod" << genId << "; EqR" << genId << "; EqI" << genId
                    << "; Ir" << genId << "; Ii" << genId << "\n";
            }
            fOut << "\n";

            fOut << "\n\t// -- Potrosaci i snage generatora --\n";
            for (int i = 0; i < nBus; ++i)
            {
                fOut << "\tPL" << buses[i].id << " = " << buses[i].pLoad << "; QL" << buses[i].id << " = " << buses[i].qLoad << "\n";
            }
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                int idx = -1;
                for (int i = 0; i < nBus; ++i) if (buses[i].id == genId) idx = i;
                fOut << "\tPgen" << genId << " = " << buses[idx].pGen << "; Vreg" << genId << " = " << buses[idx].vSet << "\n";
            }
            fOut << "\tvr1 = " << buses[0].vSet << "; vi1 = 0.0  // slack napon\n\n";

            // dinamicki parametri generatora i PSS/AVR
            fOut << "\t// -- Dinamicki parametri generatora --\n";
            fOut << "\tf0 = 50\n";
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                int pssType = getPssTypeForGenerator(genPssMap, genId);

                fOut << "\tH" << genId << " = 5.0; D" << genId << " = 2.0; Xdp" << genId << " = 0.3; Tdo" << genId << " = 6.0\n";
                fOut << "\tPm" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].pGen << "\n";

                if (pssType == 1)
                {
                    fOut << "\t// PSS Type I (Δω): Ks, Tw (wash-out), T1,T2 (lead-lag)\n";
                    fOut << "\tKs" << genId << " = " << options.ks1 << "; Tw" << genId << " = " << options.tw1
                        << "; T1" << genId << " = " << options.t1_1 << "; T2" << genId << " = " << options.t2_1 << "\n";
                    fOut << "\tKa" << genId << " = 5.0; Ta" << genId << " = 0.5; Tr" << genId << " = 0.02; Vref" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
                else if (pssType == 2)
                {
                    fOut << "\t// PSS Type II (Pe): Ks, Tw (wash-out + integrator), T1,T2 (lead-lag)\n";
                    fOut << "\tKs" << genId << " = " << options.ks2 << "; Tw" << genId << " = " << options.tw2
                        << "; T1" << genId << " = " << options.t1_2 << "; T2" << genId << " = " << options.t2_2 << "\n";
                    fOut << "\tKa" << genId << " = 5.0; Ta" << genId << " = 0.5; Tr" << genId << " = 0.02; Vref" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
                else if (pssType == 3)
                {
                    fOut << "\t// PSS Type III + AVR (Δω i Pe dvokanalni): Ks,Tw,T1,T2 + Ka,Ta(AVR)\n";
                    fOut << "\tKs" << genId << " = " << options.ks3 << "; Tw" << genId << " = " << options.tw3
                        << "; T1" << genId << " = " << options.t1_3 << "; T2" << genId << " = " << options.t2_3 << "\n";
                    fOut << "\tKa" << genId << " = 5.0; Ta" << genId << " = 0.5"
                        << "; Tr" << genId << " = 0.02; Vref" << genId << " = " << buses[findBusIndex(buses, nBus, genId)].vSet << "\n";
                }
            }
            fOut << "\n";

            progress = 55;

            // ---- Params: Ybus elementi (popunjeni iz natID dense::DblMatrix) ----
            // Umjesto racunanja u PreProc-u, admitanse citamo direktno iz Ybus
            // matrice koju smo vec sastavili koristeci natID dense matrice (buildYbus).
            fOut << "\t// -- Ybus elementi (iz natID dense::DblMatrix Ybus proracuna) --\n";
            for (int i = 0; i < nBus; ++i)
            {
                int bi = buses[i].id;
                for (int j = 0; j < nBus; ++j)
                {
                    int bj = buses[j].id;
                    double gVal = Gm(bi, bj);
                    double bVal = Bm(bi, bj);
                    if (gVal != 0.0 || bVal != 0.0)
                        fOut << "\tG" << bi << "_" << bj << " = " << gVal << "; B" << bi << "_" << bj << " = " << bVal << "\n";
                }
            }
            fOut << "\n";

            progress = 65;

            // -------------------- NLEs: mrezne jednacine tokova snaga --------------------
            // Standardna formulacija preko Ybus matrice (G,B), sumiranje po svim
            // sabirnicama j koje imaju nenulti element u Ybus za sabirnicu i.
            fOut << "NLEs:\n";
            for (int i = 0; i < nBus; ++i)
            {
                int bid = buses[i].id;
                if (buses[i].isSlack)
                    continue;

                fOut << "\t// Bilans aktivne/reaktivne snage za sabirnicu " << bid << " (Ybus pristup)\n";
                fOut << "\t(";
                bool first = true;
                for (int j = 0; j < nBus; ++j)
                {
                    int bj = buses[j].id;
                    if (Gm(bid, bj) == 0.0 && Bm(bid, bj) == 0.0)
                        continue;
                    if (!first) fOut << " + ";
                    first = false;
                    fOut << "(G" << bid << "_" << bj << "*vr" << bj << " - B" << bid << "_" << bj << "*vi" << bj << ")*vr" << bid
                        << " + (G" << bid << "_" << bj << "*vi" << bj << " + B" << bid << "_" << bj << "*vr" << bj << ")*vi" << bid;
                }
                fOut << ") = " << ((buses[i].isPV) ? ("Pgen" + std::to_string(bid)) : ("-PL" + std::to_string(bid))) << "\n";

                if (buses[i].isPV)
                {
                    fOut << "\tvr" << bid << "^2 + vi" << bid << "^2 = Vreg" << bid << "^2\n";
                }
                else
                {
                    fOut << "\t(";
                    first = true;
                    for (int j = 0; j < nBus; ++j)
                    {
                        int bj = buses[j].id;
                        if (Gm(bid, bj) == 0.0 && Bm(bid, bj) == 0.0)
                            continue;
                        if (!first) fOut << " + ";
                        first = false;
                        fOut << "(G" << bid << "_" << bj << "*vi" << bj << " + B" << bid << "_" << bj << "*vr" << bj << ")*vr" << bid
                            << " - (G" << bid << "_" << bj << "*vr" << bj << " - B" << bid << "_" << bj << "*vi" << bj << ")*vi" << bid;
                    }
                    fOut << ") = -QL" << bid << "\n";
                }
            }
            fOut << "\n";

            progress = 75;

            // -------------------- ODEs: dinamika generatora + PSS + AVR --------------------
            fOut << "ODEs:\n";
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                int pssType = getPssTypeForGenerator(genPssMap, genId);

                // Swing jednacina
                fOut << "\tdelta" << genId << "' = (omega" << genId << " - 1.0) * 2 * pi * f0\n";
                fOut << "\tomega" << genId << "' = (1/(2*H" << genId << ")) * (Pm" << genId << " - Pe" << genId << " - D" << genId << "*(omega" << genId << "-1.0))\n";
                fOut << "\tEqp" << genId << "' = 0\n"; // Eqp konstantno - garantuje stabilnost swing jednacine

                if (pssType == 1) // Type I: ulaz Δω -> wash-out -> lead-lag -> AVR (sa naponskim senzorom)
                {
                    fOut << "\txw" << genId << "' = (1/Tw" << genId << ") * ((omega" << genId << "-1.0) - xw" << genId << ")\n";
                    fOut << "\tVm" << genId << "' = (1/Tr" << genId << ") * (Vmod" << genId << " - Vm" << genId << ")\n";
                    fOut << "\tEfd" << genId << "' = (1/Ta" << genId << ") * (Ka" << genId << "*(Vref" << genId << " - Vm" << genId
                        << " + Ks" << genId << "*(T1" << genId << "/T2" << genId << ")*((omega" << genId << "-1.0)-xw" << genId << ")) - Efd" << genId << ")\n";
                }
                else if (pssType == 2) // Type II: ulaz Pe -> integrator/wash-out -> lead-lag -> AVR (sa naponskim senzorom)
                {
                    fOut << "\txw" << genId << "' = (1/Tw" << genId << ") * (Pe" << genId << " - xw" << genId << ")\n";
                    fOut << "\txi" << genId << "' = Ks" << genId << " * (Pe" << genId << " - xw" << genId << ")\n";
                    fOut << "\tVm" << genId << "' = (1/Tr" << genId << ") * (Vmod" << genId << " - Vm" << genId << ")\n";
                    fOut << "\tEfd" << genId << "' = (1/Ta" << genId << ") * (Ka" << genId << "*(Vref" << genId << " - Vm" << genId
                        << " + (T1" << genId << "/T2" << genId << ")*xi" << genId << ") - Efd" << genId << ")\n";
                }
                else if (pssType == 3) // Type III + AVR: dvokanalni Δω+Pe, pravi AVR sa senzorom napona
                {
                    fOut << "\txw" << genId << "' = (1/Tw" << genId << ") * (((omega" << genId << "-1.0) + Pe" << genId << ") - xw" << genId << ")\n";
                    fOut << "\tVm" << genId << "' = (1/Tr" << genId << ") * (Vmod" << genId << " - Vm" << genId << ")\n";
                    fOut << "\tEfd" << genId << "' = (1/Ta" << genId << ") * (Ka" << genId << "*(Vref" << genId << " - Vm" << genId
                        << " + Ks" << genId << "*(T1" << genId << "/T2" << genId << ")*(((omega" << genId << "-1.0)+Pe" << genId << ")-xw" << genId << ")) - Efd" << genId << ")\n";
                }
            }
            fOut << "\n";

            // -------------------- PostProc: izvedene velicine (Pe, Vmod...) --------------------
            fOut << "PostProc:\n";
            for (int g = 0; g < genCount; ++g)
            {
                int genId = genBuses[g];
                fOut << "\tEqR" << genId << " = Eqp" << genId << " * cos(delta" << genId << ")\n";
                fOut << "\tEqI" << genId << " = Eqp" << genId << " * sin(delta" << genId << ")\n";
                fOut << "\tIr" << genId << " = (EqI" << genId << " - vi" << genId << ") / Xdp" << genId << "\n";
                fOut << "\tIi" << genId << " = -(EqR" << genId << " - vr" << genId << ") / Xdp" << genId << "\n";
                fOut << "\tPe" << genId << " = vr" << genId << "*Ir" << genId << " + vi" << genId << "*Ii" << genId << "\n";
                fOut << "\tVmod" << genId << " = sqrt(vr" << genId << "^2 + vi" << genId << "^2)\n";
            }
            fOut << "end\n";

            fOut.close();

            progress = 85;

            // -------------------- Visual model (.vmodl) za grafike --------------------
            td::String strVisualFileName = fo::replaceFileExtension<false>(outFileName, ".vmodl");
            std::ofstream fVisual;
            if (fo::createTextFile(fVisual, strVisualFileName))
            {
                fVisual << "Header:\n\tnewTab = false\n\tdrawPlots = true\nend\n";
                fVisual << "Model [name=\"PSS Simulation Results\"]:\n";
                fVisual << "Plots [backColor=auto]:\n";

                fVisual << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Rotor angle [rad]\" name=\"Rotor angles\" legend=true]:\n";
                fVisual << "\t\t@x << t\n";
                for (int g = 0; g < genCount; ++g)
                {
                    int genId = genBuses[g];
                    const char* colors[] = { "red","cyan" };
                    fVisual << "\t\t@y << delta" << genId << " [colorL=black colorD=" << colors[g % 2] << " width=2 name=\"delta" << genId << "\"]\n";
                }
                fVisual << "\tend\n";

                fVisual << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Speed [p.u.]\" name=\"Rotor speeds\" legend=true]:\n";
                fVisual << "\t\t@x << t\n";
                for (int g = 0; g < genCount; ++g)
                {
                    int genId = genBuses[g];
                    const char* colors[] = { "green","magenta" };
                    fVisual << "\t\t@y << omega" << genId << " [colorL=darkBlue colorD=" << colors[g % 2] << " width=2 name=\"omega" << genId << "\"]\n";
                }
                fVisual << "\tend\n";

                fVisual << "\tlinePlot [xLabel=\"Time [s]\" yLabel=\"Efd [p.u.]\" name=\"AVR Excitation\" legend=true]:\n";
                fVisual << "\t\t@x << t\n";
                for (int g = 0; g < genCount; ++g)
                {
                    int genId = genBuses[g];
                    int pssType = getPssTypeForGenerator(genPssMap, genId);
                    if (pssType != 0)
                    {
                        const char* colors[] = { "orange","purple" };
                        fVisual << "\t\t@y << Efd" << genId << " [colorL=darkRed colorD=" << colors[g % 2] << " width=2 name=\"Efd" << genId << "\"]\n";
                    }
                }
                fVisual << "\tend\n";
                fVisual << "end\n";
                fVisual.close();
            }

            progress = 100;
            success = true;
            done = true;
        });

    convThread.join();
    progressThread.join();

    if (success)
        status = "OK! PSS model successfully created.";

    return success;
}