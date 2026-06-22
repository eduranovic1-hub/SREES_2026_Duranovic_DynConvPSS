#pragma once
#include <compiler/Definitions.h>
#include <sc/IPlugin.h>
#include <gui/LineEdit.h>

#ifdef MU_WINDOWS
#ifdef PLUGIN_EXPORTS
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API __declspec(dllimport)
#endif
#else
#ifdef PLUGIN_EXPORTS
#define PLUGIN_API __attribute__((visibility("default")))
#else
#define PLUGIN_API
#endif
#endif

// Tip PSS stabilizatora po generatoru
enum class PSSType { Standard = 0, TypeI, TypeII, TypeIII_AVR };

using Options = struct _Options
{
    td::String modelName;
    td::INT4 caseNumber;       // 9, 30, 118 ili 300
    td::INT4 maxIter;
    float dTime;
    float endTime;

    // Koji generator (po broju) je kojeg PSS tipa
    td::String genPssMap;      // npr. "1:1,2:2,3:3" -> generator:tip

    // PSS Type I parametri (ulaz: Δω)
    double ks1;
    double tw1;
    double t1_1, t2_1;

    // PSS Type II parametri (ulaz: Pe)
    double ks2;
    double tw2;
    double t1_2, t2_2;

    // PSS Type III + AVR parametri (ulaz: Δω i Pe)
    double ks3;
    double tw3;
    double t1_3, t2_3;
    double ka, ta;             // AVR pojačanje i vremenska konstanta
};

void onClosedPluginWindow();

bool createModel(const td::String& inputFileName, const td::String& outFileName, sc::IPlugin* pIPlugin, const Options& options, gui::LineEdit& status); //in PSSPlugin.cpp