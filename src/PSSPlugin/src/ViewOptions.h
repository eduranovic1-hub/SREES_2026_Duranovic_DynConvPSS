#pragma once
#include <gui/View.h>
#include <gui/Label.h>
#include <gui/LineEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include "PSSPlugin.h"

class ViewOptions : public gui::View
{
protected:
    gui::Label _lblName;
    gui::LineEdit _editName;
    gui::Label _lblCase;
    gui::LineEdit _editCase;

    gui::Label _lblMaxIter;
    gui::LineEdit _editMaxIter;
    gui::Label _lbldT;
    gui::LineEdit _editDeltaTime;
    gui::Label _lblEndT;
    gui::LineEdit _editEndTime;

    gui::Label _lblGenMap;
    gui::LineEdit _editGenMap;

    gui::Label _lblKs1;
    gui::LineEdit _editKs1;
    gui::Label _lblTw1;
    gui::LineEdit _editTw1;
    gui::Label _lblT1_1;
    gui::LineEdit _editT1_1;
    gui::Label _lblT2_1;
    gui::LineEdit _editT2_1;

    gui::Label _lblKs2;
    gui::LineEdit _editKs2;
    gui::Label _lblTw2;
    gui::LineEdit _editTw2;
    gui::Label _lblT1_2;
    gui::LineEdit _editT1_2;
    gui::Label _lblT2_2;
    gui::LineEdit _editT2_2;

    gui::Label _lblKs3;
    gui::LineEdit _editKs3;
    gui::Label _lblTw3;
    gui::LineEdit _editTw3;
    gui::Label _lblT1_3;
    gui::LineEdit _editT1_3;
    gui::Label _lblT2_3;
    gui::LineEdit _editT2_3;
    gui::Label _lblKa;
    gui::LineEdit _editKa;
    gui::Label _lblTa;
    gui::LineEdit _editTa;

    gui::GridLayout _gl;
    Options _options;

public:
    ViewOptions()
        : _lblName(tr("Model name:"))
        , _lblCase(tr("Case (9/30/118/300):"))
        , _lblMaxIter(tr("Max iters:"))
        , _lbldT(tr("dT [s]:"))
        , _lblEndT(tr("End time [s]:"))
        , _lblGenMap(tr("Gen:PSSType map (npr. 2:1,3:3):"))
        , _lblKs1(tr("Type I  Ks:"))
        , _lblTw1(tr("Type I  Tw [s]:"))
        , _lblT1_1(tr("Type I  T1 [s]:"))
        , _lblT2_1(tr("Type I  T2 [s]:"))
        , _lblKs2(tr("Type II  Ks:"))
        , _lblTw2(tr("Type II  Tw [s]:"))
        , _lblT1_2(tr("Type II  T1 [s]:"))
        , _lblT2_2(tr("Type II  T2 [s]:"))
        , _lblKs3(tr("Type III  Ks:"))
        , _lblTw3(tr("Type III  Tw [s]:"))
        , _lblT1_3(tr("Type III  T1 [s]:"))
        , _lblT2_3(tr("Type III  T2 [s]:"))
        , _lblKa(tr("AVR  Ka:"))
        , _lblTa(tr("AVR  Ta [s]:"))
        , _gl(20, 2)
    {
        _editName = "PSS Dynamic Model";
        _editCase = "9";
        _editMaxIter = "20";
        _editDeltaTime = "0.01";
        _editEndTime = "20.0";
        _editGenMap = "2:1,3:3";

        _editKs1 = "5.0";
        _editTw1 = "10.0";
        _editT1_1 = "0.05";
        _editT2_1 = "0.02";

        _editKs2 = "5.0";
        _editTw2 = "10.0";
        _editT1_2 = "0.05";
        _editT2_2 = "0.02";

        _editKs3 = "8.0";
        _editTw3 = "10.0";
        _editT1_3 = "0.05";
        _editT2_3 = "0.02";
        _editKa = "200.0";
        _editTa = "0.02";

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblName);     gc.appendCol(_editName);
        gc.appendRow(_lblCase);     gc.appendCol(_editCase);
        gc.appendRow(_lblMaxIter);  gc.appendCol(_editMaxIter);
        gc.appendRow(_lbldT);       gc.appendCol(_editDeltaTime);
        gc.appendRow(_lblEndT);     gc.appendCol(_editEndTime);
        gc.appendRow(_lblGenMap);   gc.appendCol(_editGenMap);

        gc.appendRow(_lblKs1);  gc.appendCol(_editKs1);
        gc.appendRow(_lblTw1);  gc.appendCol(_editTw1);
        gc.appendRow(_lblT1_1); gc.appendCol(_editT1_1);
        gc.appendRow(_lblT2_1); gc.appendCol(_editT2_1);

        gc.appendRow(_lblKs2);  gc.appendCol(_editKs2);
        gc.appendRow(_lblTw2);  gc.appendCol(_editTw2);
        gc.appendRow(_lblT1_2); gc.appendCol(_editT1_2);
        gc.appendRow(_lblT2_2); gc.appendCol(_editT2_2);

        gc.appendRow(_lblKs3);  gc.appendCol(_editKs3);
        gc.appendRow(_lblTw3);  gc.appendCol(_editTw3);
        gc.appendRow(_lblT1_3); gc.appendCol(_editT1_3);
        gc.appendRow(_lblT2_3); gc.appendCol(_editT2_3);
        gc.appendRow(_lblKa);   gc.appendCol(_editKa);
        gc.appendRow(_lblTa);   gc.appendCol(_editTa);

        setLayout(&_gl);
    }

    const Options& getOptions()
    {
        _options.modelName = _editName.getText();
        _options.caseNumber = td::INT4(std::atoi(_editCase.getText().c_str()));
        _options.maxIter = std::atoi(_editMaxIter.getText().c_str());
        _options.dTime = float(std::atof(_editDeltaTime.getText().c_str()));
        _options.endTime = float(std::atof(_editEndTime.getText().c_str()));
        _options.genPssMap = _editGenMap.getText();

        _options.ks1 = std::atof(_editKs1.getText().c_str());
        _options.tw1 = std::atof(_editTw1.getText().c_str());
        _options.t1_1 = std::atof(_editT1_1.getText().c_str());
        _options.t2_1 = std::atof(_editT2_1.getText().c_str());

        _options.ks2 = std::atof(_editKs2.getText().c_str());
        _options.tw2 = std::atof(_editTw2.getText().c_str());
        _options.t1_2 = std::atof(_editT1_2.getText().c_str());
        _options.t2_2 = std::atof(_editT2_2.getText().c_str());

        _options.ks3 = std::atof(_editKs3.getText().c_str());
        _options.tw3 = std::atof(_editTw3.getText().c_str());
        _options.t1_3 = std::atof(_editT1_3.getText().c_str());
        _options.t2_3 = std::atof(_editT2_3.getText().c_str());
        _options.ka = std::atof(_editKa.getText().c_str());
        _options.ta = std::atof(_editTa.getText().c_str());

        return _options;
    }
};