// FLTK port of openmpt/mptrack/SampleEditorDialogs.cpp

#include "stdafx.h"
#include <cfloat>
#include "ui/Ui.h"
#include "SampleEditorDialogs.h"
#include "Mptrack.h"
#include "Moddoc.h"
#include "Reporting.h"
#include "resource.h"
#include "../common/misc_util.h"
#include "../soundlib/Snd_defs.h"
#include "../soundlib/ModSample.h"
#include "ProgressDialog.h"


OPENMPT_NAMESPACE_BEGIN

static void HandleSampleLengthUnitChange(Spinner &editBox, SampleLengthUnit newUnit, uint32 sampleRate)
{
	editBox.SetAllowFractions(newUnit == SampleLengthUnit::Milliseconds);
	editBox.SetAccessibleSuffix((newUnit == SampleLengthUnit::Samples) ? UL_("samples") : UL_("ms"));
	if(newUnit == SampleLengthUnit::Samples)
	{
		// Convert from milliseconds to samples
		const double ms = editBox.GetValue();
		editBox.SetValue(mpt::saturate_round<SmpLength>(ms * sampleRate / 1000.0));
	} else
	{
		// Convert from samples to milliseconds
		const double duration = editBox.GetValue();
		editBox.SetValue(duration * 1000.0 / sampleRate);
	}
}


static void PopulateSampleLengthUnitComboBox(ComboBox &comboBox, SampleLengthUnit unit)
{
	comboBox.SetItemData(comboBox.AddString(UL_("samples")), static_cast<uintptr_t>(SampleLengthUnit::Samples));
	comboBox.SetItemData(comboBox.AddString(UL_("ms")), static_cast<uintptr_t>(SampleLengthUnit::Milliseconds));
	comboBox.SetCurSel(static_cast<int>(unit));
}


static void HandleAmplificationUnitChange(Spinner &editBox, int32 spinMin, int32 spinMax, AmplificationUnit newUnit)
{
	double value = editBox.GetValue();
	if(newUnit == AmplificationUnit::Decibels)
	{
		if(value > 0)
			value = CModDoc::LinearToDecibels(value, 100.0);
		else
			value = SampleEdit::SILENCE_DB;
	} else
	{
		if(value > SampleEdit::SILENCE_DB)
			value = CModDoc::DecibelsToLinear(value, 100.0);
		else
			value = 0;
	}
	editBox.SetRange32(spinMin, spinMax);
	editBox.SetValue(value);
}


static void PopulateAmplificationUnitComboBox(ComboBox &comboBox, AmplificationUnit unit)
{
	comboBox.SetItemData(comboBox.AddString(UL_("Percent (%)")), static_cast<uintptr_t>(AmplificationUnit::Percent));
	comboBox.SetItemData(comboBox.AddString(UL_("Decibels (dB)")), static_cast<uintptr_t>(AmplificationUnit::Decibels));
	comboBox.SetCurSel(static_cast<int>(unit));
}

//////////////////////////////////////////////////////////////////////////
// Sample amplification dialog

UI_MESSAGE_MAP_BEGIN(CAmpDlg, DialogBase)
	UI_NOTIFY(ui::EditChange, IDC_EDIT2,      &CAmpDlg::EnableFadeIn)
	UI_NOTIFY(ui::EditChange, IDC_EDIT3,      &CAmpDlg::EnableFadeOut)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &CAmpDlg::OnUnitChanged)
UI_MESSAGE_MAP_END()

void CAmpDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_fadeBox);
	pDX->BindControl(IDC_COMBO2, m_unitBox);
	pDX->BindControl(IDC_EDIT1, m_edit[0]);
	pDX->BindControl(IDC_EDIT2, m_edit[1]);
	pDX->BindControl(IDC_EDIT3, m_edit[2]);
	pDX->BindControl(IDC_UNIT1, m_unitLabel[0]);
	pDX->BindControl(IDC_UNIT2, m_unitLabel[1]);
	pDX->BindControl(IDC_UNIT3, m_unitLabel[2]);
}

CAmpDlg::CAmpDlg(Wnd *parent, AmpSettings &settings, double factorMin, double factorMax)
	: DialogBase{IDD_SAMPLE_AMPLIFY, parent}
	, m_settings{settings}
	, m_factorMinLinear{factorMin}
	, m_factorMaxLinear{factorMax}
	, m_factorMinDecibels{(factorMin > 0) ? CModDoc::LinearToDecibels(factorMin, 100.0) : SampleEdit::SILENCE_DB}
	, m_factorMaxDecibels{(factorMax > 0) ? CModDoc::LinearToDecibels(factorMax, 100.0) : SampleEdit::SILENCE_DB}
{}

bool CAmpDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_unit = m_settings.unit;
	PopulateAmplificationUnitComboBox(m_unitBox, m_unit);

	const bool allowNegative = m_factorMinLinear < 0 || m_unit == AmplificationUnit::Decibels;
	const int32 factorMin = mpt::saturate_round<int32>((m_unit == AmplificationUnit::Decibels) ? m_factorMinDecibels : m_factorMinLinear);
	const int32 factorMax = mpt::saturate_round<int32>((m_unit == AmplificationUnit::Decibels) ? m_factorMaxDecibels : m_factorMaxLinear);
	std::array<double, 3> values = {m_settings.factor, m_settings.fadeInStart, m_settings.fadeOutEnd};

	for(size_t i = 0; i < 3; i++)
	{
		m_edit[i].SetAllowFractions(true);

		if(m_unit == AmplificationUnit::Decibels)
			values[i] = (values[i] > 0) ? CModDoc::LinearToDecibels(values[i], 100.0) : SampleEdit::SILENCE_DB;
		m_edit[i].SetRange32(factorMin, factorMax);
		m_edit[i].SetValue(values[i]);
	}

	const struct
	{
		const mpt::uchar *name;
		Fade::Law id;
	} fadeLaws[] =
	{
		{ UL_("Linear"),       Fade::kLinear },
		{ UL_("Exponential"),  Fade::kPow },
		{ UL_("Square Root"),  Fade::kSqrt },
		{ UL_("Logarithmic"),  Fade::kLog },
		{ UL_("Quarter Sine"), Fade::kQuarterSine },
		{ UL_("Half Sine"),    Fade::kHalfSine },
	};

	// Add fade laws to list
	for(int i = 0; i < static_cast<int>(std::size(fadeLaws)); i++)
	{
		m_fadeBox.InsertItem(i, fadeLaws[i].name, i, fadeLaws[i].id);
		if(fadeLaws[i].id == m_settings.fadeLaw) m_fadeBox.SetCurSel(i);
	}

	OnDPIChanged();
	UpdateUnitLabels();

	m_locked = false;

	return true;
}


void CAmpDlg::OnDPIChanged()
{
	// Create icons for fade laws
	const int items = m_fadeBox.GetCount();
	const int iconSize = ui::ScalePixels(16, this);
	const int imgWidth = iconSize * items;
	std::vector<ColorRef> bits(imgWidth * iconSize, RGB(255, 0, 255));
	const ColorRef col = ui::GetSystemColor(ui::SysColor::WindowText);
	for(int i = 0, baseX = 0; i < items; i++, baseX += iconSize)
	{
		Fade::Func fadeFunc = Fade::GetFadeFunc(static_cast<Fade::Law>(m_fadeBox.GetItemData(i)));
		int oldVal = iconSize - 1;
		for(int x = 0; x < iconSize; x++)
		{
			int val = iconSize - 1 - mpt::saturate_round<int>(iconSize * fadeFunc(static_cast<double>(x) / iconSize));
			Limit(val, 0, iconSize - 1);
			if(oldVal > val && x > 0)
			{
				int dy = (oldVal - val) / 2;
				for(int y = oldVal * imgWidth; dy != 0; y -= imgWidth, dy--)
				{
					bits[baseX + (x - 1) + y] = col;
				}
				oldVal -= dy + 1;
			}
			for(int y = oldVal * imgWidth; y >= val * imgWidth; y -= imgWidth)
			{
				bits[baseX + x + y] = col;
			}
			oldVal = val;
		}
	}
	ui::Bitmap bitmap(imgWidth, iconSize);
	bitmap.SetHasAlpha(true);
	uint32 *pixels = bitmap.GetPixels();
	for(size_t i = 0; i < bits.size(); i++)
	{
		const ColorRef c = bits[i];
		pixels[i] = (c == RGB(255, 0, 255)) ? 0u : (0xFF000000u | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c));
	}
	m_list.Create(iconSize, iconSize);
	m_list.AddStrip(bitmap);
	m_fadeBox.SetImageList(&m_list);
}


void CAmpDlg::OnDestroy()
{
	m_list.RemoveAll();
}


void CAmpDlg::OnOK()
{
	std::array<double, 3> values;
	for(size_t i = 0; i < 3; i++)
	{
		values[i] = m_edit[i].GetValue();
		if(m_unit == AmplificationUnit::Decibels)
			values[i] = CModDoc::DecibelsToLinear(values[i], 100.0);
		Limit(values[i], m_factorMinLinear, m_factorMaxLinear);
	}
	m_settings.factor = values[0];
	m_settings.fadeInStart = values[1];
	m_settings.fadeOutEnd = values[2];

	m_settings.unit = m_unit;
	m_settings.fadeIn = (IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff);
	m_settings.fadeOut = (IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff);
	m_settings.fadeLaw = static_cast<Fade::Law>(m_fadeBox.GetItemData(m_fadeBox.GetCurSel()));
	DialogBase::OnOK();
}


void CAmpDlg::EnableFadeIn()
{
	if(!m_locked)
		CheckDlgButton(IDC_CHECK1, ui::CheckOn);
}


void CAmpDlg::EnableFadeOut()
{
	if(!m_locked)
		CheckDlgButton(IDC_CHECK2, ui::CheckOn);
}


void CAmpDlg::OnUnitChanged()
{
	if(m_locked)
		return;

	const AmplificationUnit newUnit = static_cast<AmplificationUnit>(m_unitBox.GetItemData(m_unitBox.GetCurSel()));
	if(newUnit == m_unit)
		return;

	m_locked = true;
	m_unit = newUnit;

	const int32 factorMin = mpt::saturate_round<int32>((m_unit == AmplificationUnit::Decibels) ? m_factorMinDecibels : m_factorMinLinear);
	const int32 factorMax = mpt::saturate_round<int32>((m_unit == AmplificationUnit::Decibels) ? m_factorMaxDecibels : m_factorMaxLinear);
	for(size_t i = 0; i < 3; i++)
	{
		HandleAmplificationUnitChange(m_edit[i], factorMin, factorMax, m_unit);
	}

	m_locked = false;
	UpdateUnitLabels();
}


void CAmpDlg::UpdateUnitLabels()
{
	const mpt::uchar *unitLabel = (m_unit == AmplificationUnit::Decibels) ? UL_("dB") : UL_("%");
	for(auto &label : m_unitLabel)
	{
		label.SetWindowText(unitLabel);
	}
}


//////////////////////////////////////////////////////////////
// Sample import dialog

SampleIO CRawSampleDlg::m_format(SampleIO::_8bit, SampleIO::mono, SampleIO::littleEndian, SampleIO::signedPCM);
SmpLength CRawSampleDlg::m_offset = 0;

UI_MESSAGE_MAP_BEGIN(CRawSampleDlg, DialogBase)
	UI_COMMAND_RANGE(IDC_RADIO1, IDC_RADIO4, &CRawSampleDlg::OnBitDepthChanged)
	UI_COMMAND_RANGE(IDC_RADIO7, IDC_RADIO10, &CRawSampleDlg::OnEncodingChanged)
	UI_COMMAND(IDC_BUTTON1, &CRawSampleDlg::OnAutodetectFormat)
UI_MESSAGE_MAP_END()


void CRawSampleDlg::DoDataExchange(DataExchange *pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_EDIT1, m_SpinOffset);
}


CRawSampleDlg::CRawSampleDlg(FileReader &file, Wnd *parent)
	: DialogBase{IDD_LOADRAWSAMPLE, parent}
	, m_file{file}
{
}


bool CRawSampleDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	if(const auto filename = m_file.GetOptionalFileName(); filename)
	{
		mpt::ustring title;
		GetWindowText(title);
		title += UL_(" - ") + filename->GetFilename().ToUnicode();
		SetWindowText(title);
	}
	m_SpinOffset.SetRange32(0, mpt::saturate_cast<int>(m_file.GetLength() - 1u));
	UpdateDialog();
	return true;
}


void CRawSampleDlg::OnOK()
{
	const int bitDepth = GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO4);
	const int channels = GetCheckedRadioButton(IDC_RADIO5, IDC_RADIO6);
	const int encoding = GetCheckedRadioButton(IDC_RADIO7, IDC_RADIO10);
	const int endianness = GetCheckedRadioButton(IDC_RADIO11, IDC_RADIO12);
	if(bitDepth == IDC_RADIO1)
		m_format |= SampleIO::_8bit;
	else if(bitDepth == IDC_RADIO2)
		m_format |= SampleIO::_16bit;
	else if(bitDepth == IDC_RADIO3)
		m_format |= SampleIO::_24bit;
	else if(bitDepth == IDC_RADIO4)
		m_format |= SampleIO::_32bit;
	if(channels == IDC_RADIO5)
		m_format |= SampleIO::mono;
	else if(channels == IDC_RADIO6)
		m_format |= SampleIO::stereoInterleaved;
	if(encoding == IDC_RADIO7)
		m_format |= SampleIO::signedPCM;
	else if(encoding == IDC_RADIO8)
		m_format |= SampleIO::unsignedPCM;
	else if(encoding == IDC_RADIO9)
		m_format |= SampleIO::deltaPCM;
	else if(encoding == IDC_RADIO10)
		m_format |= SampleIO::floatPCM;
	if(endianness == IDC_RADIO11)
		m_format |= SampleIO::littleEndian;
	else if(endianness == IDC_RADIO12)
		m_format |= SampleIO::bigEndian;
	m_rememberFormat = IsDlgButtonChecked(IDC_CHK_REMEMBERSETTINGS) != ui::CheckOff;
	m_offset = GetDlgItemInt(IDC_EDIT1, nullptr, false);
	DialogBase::OnOK();
}


void CRawSampleDlg::UpdateDialog()
{
	const int bitDepthID = IDC_RADIO1 + m_format.GetBitDepth() / 8 - 1;
	CheckRadioButton(IDC_RADIO1, IDC_RADIO4, bitDepthID);
	CheckRadioButton(IDC_RADIO5, IDC_RADIO6, (m_format.GetChannelFormat() == SampleIO::mono) ? IDC_RADIO5 : IDC_RADIO6);
	int encodingID = IDC_RADIO7;
	switch(m_format.GetEncoding())
	{
	case SampleIO::signedPCM: encodingID = IDC_RADIO7; break;
	case SampleIO::unsignedPCM: encodingID = IDC_RADIO8; break;
	case SampleIO::deltaPCM: encodingID = IDC_RADIO9; break;
	case SampleIO::floatPCM: encodingID = IDC_RADIO10; break;
	default: MPT_ASSERT_NOTREACHED();
	}
	CheckRadioButton(IDC_RADIO7, IDC_RADIO10, encodingID);
	CheckRadioButton(IDC_RADIO11, IDC_RADIO12, (m_format.GetEndianness() == SampleIO::littleEndian) ? IDC_RADIO11 : IDC_RADIO12);
	CheckDlgButton(IDC_CHK_REMEMBERSETTINGS, (m_rememberFormat ? ui::CheckOn : ui::CheckOff));
	SetDlgItemInt(IDC_EDIT1, m_offset, false);

	OnBitDepthChanged(bitDepthID);
	OnEncodingChanged(encodingID);
}


void CRawSampleDlg::OnBitDepthChanged(uint32 id)
{
	const auto bits = (id - IDC_RADIO1 + 1) * 8;
	// 8-bit: endianness doesn't matter
	bool enableEndianness = (bits == 8) ? false : true;
	GetDlgItem(IDC_RADIO11)->EnableWindow(enableEndianness);
	GetDlgItem(IDC_RADIO12)->EnableWindow(enableEndianness);
	if(bits == 8)
		CheckRadioButton(IDC_RADIO11, IDC_RADIO12, IDC_RADIO11);

	const bool hasUnsignedDelta = (bits <= 16) ? true : false;
	const bool hasFloat = (bits == 32) ? true : false;

	GetDlgItem(IDC_RADIO8)->EnableWindow(hasUnsignedDelta);
	GetDlgItem(IDC_RADIO9)->EnableWindow(hasUnsignedDelta);
	GetDlgItem(IDC_RADIO10)->EnableWindow(hasFloat);

	const int encoding = GetCheckedRadioButton(IDC_RADIO7, IDC_RADIO10);
	if((encoding == IDC_RADIO8 && !hasUnsignedDelta)
	   || (encoding == IDC_RADIO9 && !hasUnsignedDelta)
	   || (encoding == IDC_RADIO10 && !hasFloat))
		CheckRadioButton(IDC_RADIO7, IDC_RADIO10, IDC_RADIO7);
}


void CRawSampleDlg::OnEncodingChanged(uint32 id)
{
	const bool isUnsignedDelta = (id == IDC_RADIO8) || (id == IDC_RADIO9);
	const bool isFloat         = (id == IDC_RADIO10);

	GetDlgItem(IDC_RADIO1)->EnableWindow(isFloat ? false : true);
	GetDlgItem(IDC_RADIO2)->EnableWindow(isFloat ? false : true);
	GetDlgItem(IDC_RADIO3)->EnableWindow((isFloat || isUnsignedDelta) ? false : true);
	GetDlgItem(IDC_RADIO4)->EnableWindow(isUnsignedDelta ? false : true);

	const int bitDepth = GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO4);
	if(bitDepth != IDC_RADIO4 && isFloat)
		CheckRadioButton(IDC_RADIO1, IDC_RADIO4, IDC_RADIO4);
	if((bitDepth == IDC_RADIO3 || bitDepth == IDC_RADIO4) && isUnsignedDelta)
		CheckRadioButton(IDC_RADIO1, IDC_RADIO4, IDC_RADIO1);
}


class AutodetectFormatDlg : public CProgressDialog
{
	CRawSampleDlg &m_parent;

public:
	SampleIO m_bestFormat;

	AutodetectFormatDlg(CRawSampleDlg &parent)
		: CProgressDialog(&parent)
		, m_parent(parent) {}

	void Run() override
	{
		// Probed raw formats... little-endian and stereo versions are automatically checked as well.
		static constexpr SampleIO ProbeFormats[] =
			{
				// 8-Bit
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::unsignedPCM},
				{SampleIO::_8bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::deltaPCM},
				// 16-Bit
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::unsignedPCM},
				{SampleIO::_16bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::deltaPCM},
				// 24-Bit
				{SampleIO::_24bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				// 32-Bit
				{SampleIO::_32bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::signedPCM},
				{SampleIO::_32bit, SampleIO::mono, SampleIO::bigEndian, SampleIO::floatPCM},
			};

		SetTitle(UL_("Raw Import"));
		SetText(UL_("Determining raw format..."));
		SetRange(0, std::size(ProbeFormats) * 4);

		double bestError = DBL_MAX;
		uint64 progress  = 0;

		for(SampleIO format : ProbeFormats)
		{
			for(const auto endianness : {SampleIO::littleEndian, SampleIO::bigEndian})
			{
				if(endianness == SampleIO::bigEndian && format.GetBitDepth() == SampleIO::_8bit)
					continue;
				format |= endianness;
				for(const auto channels : {SampleIO::mono, SampleIO::stereoInterleaved})
				{
					format |= channels;

					ModSample sample;
					m_parent.m_file.Seek(m_parent.m_offset);
					const auto bytesPerSample = format.GetNumChannels() * format.GetBitDepth() / 8u;
					sample.nLength            = mpt::saturate_cast<SmpLength>(m_parent.m_file.BytesLeft() / bytesPerSample);
					if(!format.ReadSample(sample, m_parent.m_file))
						continue;

					const uint8 numChannels = sample.GetNumChannels();
					double error            = 0.0;
					for(uint8 chn = 0; chn < numChannels; chn++)
					{
						const auto ComputeSampleError = [](auto *v, SmpLength length, uint8 numChannels)
						{
							const double factor = 1.0 / (1u << (sizeof(*v) * 8u - 1u));
							double error        = 0.0;
							int32 prev          = 0;
							for(SmpLength i = length; i != 0; i--, v += numChannels)
							{
								auto diff = (*v - prev) * factor;
								error += diff * diff;
								prev = *v;
							}
							return error;
						};

						if(sample.uFlags[CHN_16BIT])
							error += ComputeSampleError(sample.sample16() + chn, sample.nLength, numChannels);
						else
							error += ComputeSampleError(sample.sample8() + chn, sample.nLength, numChannels);
					}
					sample.FreeSample();

					double errorFactor = format.GetBitDepth() * format.GetBitDepth() / 64;
					// Delta PCM often produces slightly worse error compared to signed PCM for real delta samples, so give it a bit of an advantage.
					if(format.GetEncoding() == SampleIO::deltaPCM)
						errorFactor *= 0.75;

					error *= errorFactor;

					if(error < bestError)
					{
						bestError  = error;
						m_bestFormat = format;
					}

					SetProgress(++progress);
					ProcessMessages();
					if(m_abort)
					{
						EndDialog(IDCANCEL);
						return;
					}
				}
			}
		}

		EndDialog(IDOK);
	}
};


void CRawSampleDlg::OnAutodetectFormat()
{
	m_offset = GetDlgItemInt(IDC_EDIT1, nullptr, false);
	AutodetectFormatDlg dlg(*this);
	if(dlg.DoModal() == IDOK)
	{
		m_format = dlg.m_bestFormat;
		UpdateDialog();
	}
}


/////////////////////////////////////////////////////////////////////////
// Add silence / resize sample dialog

UI_MESSAGE_MAP_BEGIN(AddSilenceDlg, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1,           &AddSilenceDlg::OnUnitChanged)
	UI_COMMAND(IDC_RADIO_ADDSILENCE_BEGIN, &AddSilenceDlg::OnEditModeChanged)
	UI_COMMAND(IDC_RADIO_ADDSILENCE_END,   &AddSilenceDlg::OnEditModeChanged)
	UI_COMMAND(IDC_RADIO_RESIZETO,         &AddSilenceDlg::OnEditModeChanged)
	UI_COMMAND(IDC_RADIO1,                 &AddSilenceDlg::OnEditModeChanged)
UI_MESSAGE_MAP_END()

SmpLength AddSilenceDlg::m_addSamples = 32;
SmpLength AddSilenceDlg::m_createSamples = 64;

void AddSilenceDlg::DoDataExchange(DataExchange *pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_COMBO1, m_ComboUnit);
	pDX->BindControl(IDC_EDIT_ADDSILENCE, m_EditAmount);
}


AddSilenceDlg::AddSilenceDlg(Wnd *parent, SmpLength origLength, uint32 sampleRate, bool allowOPL)
	: DialogBase{IDD_ADDSILENCE, parent}
	, m_numSamples{m_addSamples}
	, m_sampleRate{sampleRate}
	, m_allowOPL{allowOPL}
{
	if(origLength > 0)
	{
		m_length = origLength;
		m_editOption = kSilenceAtEnd;
	} else
	{
		m_length = m_createSamples;
		m_editOption = kResize;
	}
}


bool AddSilenceDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_EditAmount.SetRange32(0, int32_max);

	m_ComboUnit.SetAccessibleName(UL_("Length unit"));
	if(m_sampleRate == 0)
	{
		// Can't do any conversions if sample rate is unknown
		m_ComboUnit.EnableWindow(false);
		m_unit = SampleLengthUnit::Samples;
	}
	PopulateSampleLengthUnitComboBox(m_ComboUnit, m_unit);

	int buttonID = IDC_RADIO_ADDSILENCE_END;
	switch(m_editOption)
	{
	case kSilenceAtBeginning: buttonID = IDC_RADIO_ADDSILENCE_BEGIN; break;
	case kSilenceAtEnd:       buttonID = IDC_RADIO_ADDSILENCE_END; break;
	case kResize:             buttonID = IDC_RADIO_RESIZETO; break;
	default: break;
	}
	CheckDlgButton(buttonID, ui::CheckOn);

	m_EditAmount.SetAllowFractions(m_unit == SampleLengthUnit::Milliseconds);
	m_EditAmount.SetAccessibleSuffix((m_unit == SampleLengthUnit::Samples) ? UL_("samples") : UL_("ms"));
	SetDlgItemInt(IDC_EDIT_ADDSILENCE, (m_editOption == kResize) ? m_length : m_numSamples, false);
	GetDlgItem(IDC_RADIO1)->EnableWindow(m_allowOPL ? true : false);

	return true;
}


void AddSilenceDlg::OnOK()
{
	m_numSamples = GetEditLength();
	switch(m_editOption = GetEditMode())
	{
	case kSilenceAtBeginning:
	case kSilenceAtEnd:
		m_addSamples = m_numSamples;
		break;
	case kResize:
		m_createSamples = m_numSamples;
		break;
	default:
		break;
	}
	DialogBase::OnOK();
}


SmpLength AddSilenceDlg::GetEditLength() const
{
	if(m_unit == SampleLengthUnit::Milliseconds)
	{
		const double ms = m_EditAmount.GetValue();
		return mpt::saturate_round<SmpLength>(ms * m_sampleRate / 1000.0);
	} else
	{
		return GetDlgItemInt(IDC_EDIT_ADDSILENCE, nullptr, false);
	}
}


void AddSilenceDlg::OnEditModeChanged()
{
	AddSilenceOptions newEditOption = GetEditMode();
	GetDlgItem(IDC_EDIT_ADDSILENCE)->EnableWindow((newEditOption == kOPLInstrument) ? false : true);
	if(newEditOption != kResize && m_editOption == kResize)
	{
		// Switch to "add silence"
		m_length = GetEditLength();
		if(m_unit == SampleLengthUnit::Milliseconds)
			m_EditAmount.SetValue(m_numSamples * 1000.0 / m_sampleRate);
		else
			SetDlgItemInt(IDC_EDIT_ADDSILENCE, m_numSamples);
	} else if(newEditOption == kResize && m_editOption != kResize)
	{
		// Switch to "resize"
		m_numSamples = GetEditLength();
		if(m_unit == SampleLengthUnit::Milliseconds)
			m_EditAmount.SetValue(m_length * 1000.0 / m_sampleRate);
		else
			SetDlgItemInt(IDC_EDIT_ADDSILENCE, m_length);
	}
	m_editOption = newEditOption;
}


void AddSilenceDlg::OnUnitChanged()
{
	const auto unit = static_cast<SampleLengthUnit>(m_ComboUnit.GetItemData(m_ComboUnit.GetCurSel()));
	if(m_unit == unit)
		return;

	m_unit = unit;
	HandleSampleLengthUnitChange(m_EditAmount, m_unit, m_sampleRate);
}


AddSilenceDlg::AddSilenceOptions AddSilenceDlg::GetEditMode() const
{
	if(IsDlgButtonChecked(IDC_RADIO_ADDSILENCE_BEGIN)) return kSilenceAtBeginning;
	else if(IsDlgButtonChecked(IDC_RADIO_ADDSILENCE_END)) return kSilenceAtEnd;
	else if(IsDlgButtonChecked(IDC_RADIO_RESIZETO)) return kResize;
	else if(IsDlgButtonChecked(IDC_RADIO1)) return kOPLInstrument;
	MPT_ASSERT_NOTREACHED();
	return kSilenceAtEnd;
}


/////////////////////////////////////////////////////////////////////////
// Sample grid dialog

UI_MESSAGE_MAP_BEGIN(CSampleGridDlg, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &CSampleGridDlg::OnUnitChanged)
	UI_NOTIFY(ui::EditSetFocus, IDC_EDIT1,    &CSampleGridDlg::OnSegmentsFocus)
	UI_NOTIFY(ui::EditSetFocus, IDC_EDIT2,    &CSampleGridDlg::OnSpacingFocus)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,      &CSampleGridDlg::OnSegmentsChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT2,      &CSampleGridDlg::OnSpacingChanged)
UI_MESSAGE_MAP_END()


void CSampleGridDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_EDIT1, m_EditSegments);
	pDX->BindControl(IDC_EDIT2, m_EditSpacing);
	pDX->BindControl(IDC_COMBO1, m_ComboUnit);
}


CSampleGridDlg::CSampleGridDlg(Wnd *parent, SampleGridMode mode, double segments, double spacing, SampleLengthUnit unit, SmpLength maxSegments, uint32 sampleRate)
	: DialogBase{IDD_SAMPLE_GRID_SIZE, parent}
	, m_mode{mode}
	, m_maxSegments{maxSegments}
	, m_segments{std::max(segments, 2.0)}
	, m_spacing{spacing}
	, m_unit{unit}
	, m_sampleRate{sampleRate > 0 ? sampleRate : 8363u}
{
}


bool CSampleGridDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_ComboUnit.SetAccessibleName(UL_("Segment length unit"));
	PopulateSampleLengthUnitComboBox(m_ComboUnit, m_unit);

	m_EditSegments.SetRange32(1, m_maxSegments);
	m_EditSegments.SetAllowFractions(true);
	m_EditSegments.SetValue(m_segments);

	m_EditSpacing.SetRange32(0, m_maxSegments);
	m_EditSpacing.SetAllowFractions(true);
	m_EditSpacing.SetValue(m_spacing);
	m_EditSpacing.SetAccessibleName(UL_("Segment length"));
	m_EditSpacing.SetAccessibleSuffix((m_unit == SampleLengthUnit::Samples) ? UL_("samples") : UL_("ms"));

	int radioChoice = IDC_RADIO1;
	switch(m_mode)
	{
	case SampleGridMode::NoGrid:
	case SampleGridMode::DivideIntoSegments:
		radioChoice = IDC_RADIO2;
		GotoDlgCtrl(&m_EditSegments);
		break;
	case SampleGridMode::DivideEveryN:
		radioChoice = IDC_RADIO3;
		GotoDlgCtrl(&m_EditSpacing);
		break;
	}
	CheckRadioButton(IDC_RADIO1, IDC_RADIO3, radioChoice);

	m_locked = false;
	return false;
}


void CSampleGridDlg::OnOK()
{
	if(IsDlgButtonChecked(IDC_RADIO1))
	{
		m_mode = SampleGridMode::NoGrid;
	} else if(IsDlgButtonChecked(IDC_RADIO2))
	{
		m_mode = SampleGridMode::DivideIntoSegments;
		m_segments = m_EditSegments.GetValue();
		if(m_segments < 1.0)
			m_mode = SampleGridMode::NoGrid;
	} else if(IsDlgButtonChecked(IDC_RADIO3))
	{
		m_mode = SampleGridMode::DivideEveryN;
		m_spacing = m_EditSpacing.GetValue();
		m_unit = static_cast<SampleLengthUnit>(m_ComboUnit.GetItemData(m_ComboUnit.GetCurSel()));
		
		double effectiveSamples = m_spacing;
		if(m_unit == SampleLengthUnit::Milliseconds)
			effectiveSamples *= m_sampleRate / 1000.0;

		if(effectiveSamples < 1.0)
			m_mode = SampleGridMode::NoGrid;
	}
	DialogBase::OnOK();
}


void CSampleGridDlg::OnUnitChanged()
{
	const auto newUnit = static_cast<SampleLengthUnit>(m_ComboUnit.GetItemData(m_ComboUnit.GetCurSel()));
	if(newUnit == m_unit)
		return;

	double val = m_EditSpacing.GetValue();
	if(newUnit == SampleLengthUnit::Milliseconds)
		val *= 1000.0 / m_sampleRate;
	else
		val *= m_sampleRate / 1000.0;
	m_unit = newUnit;
	m_locked = true;
	m_EditSpacing.SetValue(val);
	m_EditSpacing.SetAccessibleSuffix((m_unit == SampleLengthUnit::Samples) ? UL_("samples") : UL_("ms"));
	m_locked = false;

	CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3);
}


void CSampleGridDlg::OnEditChanged(int radio, bool onlyMouse)
{
	if(m_locked || m_lastInputDevice == InputDevice::Unknown)
		return;
	if(!onlyMouse || m_lastInputDevice == InputDevice::Mouse)
		CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + radio);
}


/////////////////////////////////////////////////////////////////////////
// Sample cross-fade dialog

uint32 CSampleXFadeDlg::m_fadeLength  = 20000;
uint32 CSampleXFadeDlg::m_fadeLaw = 50000;
bool CSampleXFadeDlg::m_afterloopFade = true;
bool CSampleXFadeDlg::m_useSustainLoop = false;

UI_MESSAGE_MAP_BEGIN(CSampleXFadeDlg, DialogBase)
	UI_COMMAND(IDC_RADIO1,  &CSampleXFadeDlg::OnLoopTypeChanged)
	UI_COMMAND(IDC_RADIO2,  &CSampleXFadeDlg::OnLoopTypeChanged)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1, &CSampleXFadeDlg::OnFadeLengthChanged)
UI_MESSAGE_MAP_END()


void CSampleXFadeDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_EDIT1, m_SpinSamples);
	pDX->BindControl(IDC_SLIDER1, m_SliderLength);
	pDX->BindControl(IDC_SLIDER2, m_SliderFadeLaw);
	pDX->BindControl(IDC_RADIO1, m_RadioNormalLoop);
	pDX->BindControl(IDC_RADIO2, m_RadioSustainLoop);
}


CSampleXFadeDlg::CSampleXFadeDlg(Wnd *parent, ModSample &sample)
	: DialogBase{IDD_SAMPLE_XFADE, parent}
	, m_sample{sample}
{
}

bool CSampleXFadeDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	const bool hasNormal = (m_sample.uFlags[CHN_LOOP] || !m_sample.uFlags[CHN_SUSTAINLOOP]) && m_sample.nLoopStart > 0;
	const bool hasSustain = m_sample.uFlags[CHN_SUSTAINLOOP] && m_sample.nSustainStart > 0;
	const bool hasBothLoops = hasNormal && hasSustain;
	m_RadioNormalLoop.EnableWindow(hasBothLoops);
	m_RadioSustainLoop.EnableWindow(hasBothLoops);
	CheckRadioButton(IDC_RADIO1, IDC_RADIO2, ((m_useSustainLoop && hasSustain) || !hasNormal) ? IDC_RADIO2 : IDC_RADIO1);

	m_SliderLength.SetRange(0, 100000);
	m_SliderLength.SetPos(m_fadeLength);
	m_SliderFadeLaw.SetRange(0, 100000);
	m_SliderFadeLaw.SetPos(m_fadeLaw);

	OnLoopTypeChanged();

	return true;
}


void CSampleXFadeDlg::OnOK()
{
	m_fadeLength = m_SliderLength.GetPos();
	m_fadeLaw = m_SliderFadeLaw.GetPos();
	m_afterloopFade = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
	m_useSustainLoop = IsDlgButtonChecked(IDC_RADIO2) != ui::CheckOff;
	Limit(m_fadeLength, uint32(0), uint32(100000));
	DialogBase::OnOK();
}


void CSampleXFadeDlg::OnLoopTypeChanged()
{
	SmpLength loopStart = m_sample.nLoopStart, loopEnd = m_sample.nLoopEnd;
	if(IsDlgButtonChecked(IDC_RADIO2))
	{
		loopStart = m_sample.nSustainStart;
		loopEnd = m_sample.nSustainEnd;
	}
	m_maxLength = std::min({ m_sample.nLength, loopStart, loopEnd / 2u });
	m_loopLength = loopEnd - loopStart;

	m_editLocked = true;
	m_SpinSamples.SetRange32(0, std::min(m_loopLength, m_maxLength));
	GotoDlgCtrl(GetDlgItem(IDC_EDIT1));
	CheckDlgButton(IDC_CHECK1, m_afterloopFade ? ui::CheckOn : ui::CheckOff);

	SmpLength numSamples = PercentToSamples(m_SliderLength.GetPos());
	numSamples = std::min({ numSamples, m_loopLength, m_maxLength });
	m_SpinSamples.SetPos(numSamples);
	SetDlgItemInt(IDC_EDIT1, numSamples, false);

	m_editLocked = false;
}


void CSampleXFadeDlg::OnFadeLengthChanged()
{
	if(m_editLocked) return;
	SmpLength numSamples = GetDlgItemInt(IDC_EDIT1, NULL, false);
	numSamples = std::min({ numSamples, m_loopLength, m_maxLength });
	m_SliderLength.SetPos(SamplesToPercent(numSamples));
}


void CSampleXFadeDlg::OnHScroll(uint32, uint32, Wnd *sb)
{
	if(sb == (ScrollBar *)(&m_SliderLength))
	{
		m_editLocked = true;
		SmpLength numSamples = PercentToSamples(m_SliderLength.GetPos());
		if(numSamples > m_maxLength)
		{
			numSamples = m_maxLength;
			m_SliderLength.SetPos(SamplesToPercent(numSamples));
		}
		m_SpinSamples.SetPos(numSamples);
		SetDlgItemInt(IDC_EDIT1, numSamples, false);
		m_editLocked = false;
	}
}


mpt::ustring CSampleXFadeDlg::GetToolTipText(uint32 id, WindowHandle) const
{
	mpt::ustring s;
	switch(id)
	{
	case IDC_SLIDER1:
		{
			uint32 percent = m_SliderLength.GetPos();
			s = ui::Format(UL_("%u.%03u%% of the loop (%u samples)"), percent / 1000, percent % 1000, PercentToSamples(percent));
		}
		break;
	case IDC_SLIDER2:
		s = UL_("Slide towards constant power for fixing badly looped samples.");
		break;
	}
	
	return s;
}


/////////////////////////////////////////////////////////////////////////
// Resampling dialog

CResamplingDlg::ResamplingOption CResamplingDlg::m_lastChoice = CResamplingDlg::Upsample;
uint32 CResamplingDlg::m_lastFrequency = 0;
bool CResamplingDlg::m_updatePatternCommands = false;
bool CResamplingDlg::m_updatePatternNotes = false;

UI_MESSAGE_MAP_BEGIN(CResamplingDlg, DialogBase)
	UI_NOTIFY(ui::EditSetFocus, IDC_EDIT1, &CResamplingDlg::OnFocusEdit)
	UI_NOTIFY(ui::EditChange, IDC_EDIT1,   &CResamplingDlg::OnEditChanged)
UI_MESSAGE_MAP_END()


CResamplingDlg::CResamplingDlg(Wnd *parent, uint32 frequency, ResamplingMode srcMode, Action action, bool allowAdjustNotes)
	: DialogBase{IDD_RESAMPLE, parent}
	, m_srcMode{srcMode}
	, m_frequency{frequency}
	, m_action{action}
	, m_allowAdjustNotes{allowAdjustNotes}
{
}


bool CResamplingDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();
	const mpt::uchar *title = nullptr;
	switch(m_action)
	{
	case Action::OneSample: title = UL_("Resample"); break;
	case Action::OneChannel: title = UL_("Resample Channel"); break;
	case Action::AllSamples: title = UL_("Resample All"); break;
	}
	SetWindowText(title);

	CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO1 + m_lastChoice);
	if(m_frequency > 0)
	{
		mpt::uchar s[32];
		wsprintf(s, UL_("&Upsample (%u Hz)"), m_frequency * 2);
		SetDlgItemText(IDC_RADIO1, s);
		wsprintf(s, UL_("&Downsample (%u Hz)"), m_frequency / 2);
		SetDlgItemText(IDC_RADIO2, s);

		if(!m_lastFrequency)
			m_lastFrequency = m_frequency;
	}
	if(!m_lastFrequency)
		m_lastFrequency = 48000;


	SetDlgItemInt(IDC_EDIT1, m_lastFrequency, false);
	Spinner *frequencySpinner = static_cast<Spinner *>(GetDlgItem(IDC_EDIT1));
	frequencySpinner->SetRange32(1, 999999);
	frequencySpinner->SetPos32(m_lastFrequency);

	ComboBox *cbnResampling = static_cast<ComboBox *>(GetDlgItem(IDC_COMBO_FILTER));
	cbnResampling->SetRedraw(false);
	const auto resamplingModes = Resampling::AllModesWithDefault();
	for(auto mode : resamplingModes)
	{
		mpt::ustring desc = UL_("r8brain (High Quality)");
		if(mode != SRCMODE_DEFAULT)
			desc = CTrackApp::GetResamplingModeName(mode, 1, true);

		int index = cbnResampling->AddString(desc);
		cbnResampling->SetItemData(index, mode);
		if(m_srcMode == mode)
			cbnResampling->SetCurSel(index);
	}
	cbnResampling->SetRedraw(true);

	CheckDlgButton(IDC_CHECK1, m_updatePatternCommands ? ui::CheckOn : ui::CheckOff);
	CheckDlgButton(IDC_CHECK2, (m_updatePatternNotes && m_allowAdjustNotes) ? ui::CheckOn : ui::CheckOff);
	GetDlgItem(IDC_CHECK1)->EnableWindow(m_action != Action::OneChannel ? true : false);
	GetDlgItem(IDC_CHECK2)->EnableWindow((m_allowAdjustNotes && m_action != Action::OneChannel) ? true : false);

	return true;
}


void CResamplingDlg::OnOK()
{
	const int choice = GetCheckedRadioButton(IDC_RADIO1, IDC_RADIO3);
	if(choice == IDC_RADIO1)
	{
		m_lastChoice = Upsample;
		m_frequency *= 2;
	} else if(choice == IDC_RADIO2)
	{
		m_lastChoice = Downsample;
		m_frequency /= 2;
	} else
	{
		m_lastChoice = Custom;
		uint32 newFrequency = GetDlgItemInt(IDC_EDIT1, NULL, false);
		if(newFrequency > 0)
		{
			m_lastFrequency = m_frequency = newFrequency;
		} else
		{
			ui::Beep();
			GotoDlgCtrl(GetDlgItem(IDC_EDIT1));
			return;
		}
	}

	ComboBox *cbnResampling = static_cast<ComboBox *>(GetDlgItem(IDC_COMBO_FILTER));
	m_srcMode = static_cast<ResamplingMode>(cbnResampling->GetItemData(cbnResampling->GetCurSel()));

	m_updatePatternCommands = IsDlgButtonChecked(IDC_CHECK1) != ui::CheckOff;
	m_updatePatternNotes = IsDlgButtonChecked(IDC_CHECK2) != ui::CheckOff;

	DialogBase::OnOK();
}


void CResamplingDlg::OnFocusEdit()
{
	if(m_lastInputDevice == InputDevice::Mouse)
		CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3);
}


void CResamplingDlg::OnEditChanged()
{
	if(m_lastInputDevice != InputDevice::Unknown)
		CheckRadioButton(IDC_RADIO1, IDC_RADIO3, IDC_RADIO3);
}


////////////////////////////////////////////////////////////////////////////////////////////
// Sample mix dialog

CMixSampleDlg::SmpLengthSigned CMixSampleDlg::sampleOffset = 0;
double CMixSampleDlg::amplifyOriginal = 50.0;
double CMixSampleDlg::amplifyMix = 50.0;
AmplificationUnit CMixSampleDlg::m_ampUnit = AmplificationUnit::Percent;
SampleLengthUnit CMixSampleDlg::m_lengthUnit = SampleLengthUnit::Samples;


UI_MESSAGE_MAP_BEGIN(CMixSampleDlg, DialogBase)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO1, &CMixSampleDlg::OnAmpUnitChanged)
	UI_NOTIFY(ui::ComboSelChange, IDC_COMBO2, &CMixSampleDlg::OnLengthUnitChanged)
UI_MESSAGE_MAP_END()


void CMixSampleDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_UNIT1, m_unitLabel[0]);
	pDX->BindControl(IDC_UNIT2, m_unitLabel[1]);
	pDX->BindControl(IDC_EDIT_SAMPVOL1, m_edit[0]);
	pDX->BindControl(IDC_EDIT_SAMPVOL2, m_edit[1]);
	pDX->BindControl(IDC_EDIT_OFFSET, m_EditOffset);
	pDX->BindControl(IDC_COMBO1, m_ampUnitBox);
	pDX->BindControl(IDC_COMBO2, m_lengthUnitBox);
}


CMixSampleDlg::CMixSampleDlg(Wnd *parent, uint32 sampleRate)
	: DialogBase(IDD_MIXSAMPLES, parent)
	, m_factorMinLinear{-100'000}
	, m_factorMaxLinear{100'000}
	, m_factorMinDecibels{(m_factorMinLinear > 0) ? CModDoc::LinearToDecibels(m_factorMinLinear, 100.0) : SampleEdit::SILENCE_DB}
	, m_factorMaxDecibels{(m_factorMaxLinear > 0) ? CModDoc::LinearToDecibels(m_factorMaxLinear, 100.0) : SampleEdit::SILENCE_DB}
	, m_sampleRate{sampleRate}
{ }


bool CMixSampleDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	m_lengthUnitBox.SetAccessibleName(UL_("Length unit"));
	if(m_sampleRate == 0)
	{
		// Can't do any conversions if sample rate is unknown
		m_lengthUnitBox.EnableWindow(false);
		m_lengthUnit = SampleLengthUnit::Samples;
	}
	PopulateSampleLengthUnitComboBox(m_lengthUnitBox, m_lengthUnit);
	PopulateAmplificationUnitComboBox(m_ampUnitBox, m_ampUnit);

	// Offset
	static_assert(MAX_SAMPLE_LENGTH <= std::numeric_limits<SmpLengthSigned>::max());
	m_EditOffset.SetRange32(-static_cast<SmpLengthSigned>(MAX_SAMPLE_LENGTH), MAX_SAMPLE_LENGTH);
	m_EditOffset.SetAccessibleSuffix((m_lengthUnit == SampleLengthUnit::Samples) ? UL_("samples") : UL_("ms"));
	m_EditOffset.SetAllowFractions(m_lengthUnit == SampleLengthUnit::Milliseconds);
	double duration = sampleOffset;
	if(m_lengthUnit == SampleLengthUnit::Milliseconds)
		duration *= 1000.0 / m_sampleRate;
	m_EditOffset.SetValue(duration);

	// Volumes
	const bool allowNegative = m_factorMinLinear < 0 || m_ampUnit == AmplificationUnit::Decibels;
	const int32 factorMin = mpt::saturate_round<int32>((m_ampUnit == AmplificationUnit::Decibels) ? m_factorMinDecibels : m_factorMinLinear);
	const int32 factorMax = mpt::saturate_round<int32>((m_ampUnit == AmplificationUnit::Decibels) ? m_factorMaxDecibels : m_factorMaxLinear);
	std::array<double, 2> values = {amplifyOriginal, amplifyMix};

	for(size_t i = 0; i < 2; i++)
	{
		m_edit[i].SetAllowFractions(true);

		if(m_ampUnit == AmplificationUnit::Decibels)
			values[i] = (values[i] > 0) ? CModDoc::LinearToDecibels(values[i], 100.0) : SampleEdit::SILENCE_DB;
		m_edit[i].SetRange32(factorMin, factorMax);
		m_edit[i].SetValue(values[i]);
	}

	UpdateUnitLabels();

	return true;
}


void CMixSampleDlg::OnOK()
{
	DialogBase::OnOK();

	double offset = m_EditOffset.GetValue();
	if(m_lengthUnit == SampleLengthUnit::Milliseconds)
		offset *= m_sampleRate / 1000.0;
	sampleOffset = Clamp(mpt::saturate_round<SmpLengthSigned>(offset), -static_cast<SmpLengthSigned>(MAX_SAMPLE_LENGTH), static_cast<SmpLengthSigned>(MAX_SAMPLE_LENGTH));

	std::array<double, 2> values;
	for(size_t i = 0; i < 2; i++)
	{
		values[i] = m_edit[i].GetValue();
		if(m_ampUnit == AmplificationUnit::Decibels)
			values[i] = CModDoc::DecibelsToLinear(values[i], 100.0);
		Limit(values[i], m_factorMinLinear, m_factorMaxLinear);
	}

	amplifyOriginal = values[0];
	amplifyMix = values[1];
}


void CMixSampleDlg::OnLengthUnitChanged()
{
	const auto unit = static_cast<SampleLengthUnit>(m_lengthUnitBox.GetItemData(m_lengthUnitBox.GetCurSel()));
	if(m_lengthUnit == unit)
		return;

	m_lengthUnit = unit;
	HandleSampleLengthUnitChange(m_EditOffset, m_lengthUnit, m_sampleRate);
}


void CMixSampleDlg::OnAmpUnitChanged()
{
	const AmplificationUnit newUnit = static_cast<AmplificationUnit>(m_ampUnitBox.GetItemData(m_ampUnitBox.GetCurSel()));
	if(newUnit == m_ampUnit)
		return;

	m_ampUnit = newUnit;

	const int32 factorMin = mpt::saturate_round<int32>((m_ampUnit == AmplificationUnit::Decibels) ? m_factorMinDecibels : m_factorMinLinear);
	const int32 factorMax = mpt::saturate_round<int32>((m_ampUnit == AmplificationUnit::Decibels) ? m_factorMaxDecibels : m_factorMaxLinear);
	for(size_t i = 0; i < 2; i++)
	{
		HandleAmplificationUnitChange(m_edit[i], factorMin, factorMax, m_ampUnit);
	}

	UpdateUnitLabels();
}


void CMixSampleDlg::UpdateUnitLabels()
{
	const mpt::uchar *unitLabel = (m_ampUnit == AmplificationUnit::Decibels) ? UL_("dB") : UL_("%");
	for(auto &label : m_unitLabel)
	{
		label.SetWindowText(unitLabel);
	}
}

OPENMPT_NAMESPACE_END
