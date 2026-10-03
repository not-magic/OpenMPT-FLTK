// FLTK port of openmpt/mptrack/AboutDialog.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "AboutDialog.h"
#include "Image.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"
#include "TrackerSettings.h"
#include "mpt/format/join.hpp"
#include "mpt/string/utility.hpp"
#include "resource.h"
#include "../common/version.h"
#include "../misc/mptWine.h"


OPENMPT_NAMESPACE_BEGIN


CAboutDlg *CAboutDlg::instance = nullptr;

UI_MESSAGE_MAP_BEGIN(CRippleBitmap, Wnd)
UI_MESSAGE_MAP_END()


CRippleBitmap::CRippleBitmap()
{
	m_bitmapSrc = LoadPixelImage(GetResource(IDB_MPTRACK));
	m_bitmapTarget = std::make_unique<RawImage>(m_bitmapSrc->Width(), m_bitmapSrc->Height());
	m_offset1.assign(m_bitmapSrc->Pixels().size(), 0);
	m_offset2.assign(m_bitmapSrc->Pixels().size(), 0);
	m_frontBuf = m_offset2.data();
	m_backBuf = m_offset1.data();

	// Pre-fill first and last row of output bitmap, since those won't be touched.
	const RawImage::Pixel *in1 = m_bitmapSrc->Pixels().data(), *in2 = m_bitmapSrc->Pixels().data() + (m_bitmapSrc->Height() - 1) * m_bitmapSrc->Width();
	RawImage::Pixel *out1 = m_bitmapTarget->Pixels().data(), *out2 = m_bitmapTarget->Pixels().data() + (m_bitmapSrc->Height() - 1) * m_bitmapSrc->Width();
	for(uint32 i = 0; i < m_bitmapSrc->Width(); i++)
	{
		*(out1++) = *(in1++);
		*(out2++) = *(in2++);
	}
}


CRippleBitmap::~CRippleBitmap()
{
}


void CRippleBitmap::OnMouseMove(uint32 nFlags, Point point)
{

	// Rate limit in order to avoid too may ripples.
	uint32 now = static_cast<uint32>(Util::GetTickCount64());
	if(now - m_lastRipple < UPDATE_INTERVAL)
		return;
	m_lastRipple = now;

	// Initiate ripples at cursor location
	point.x = ui::ScalePixelsInv(point.x, this);
	point.y = ui::ScalePixelsInv(point.y, this);
	Limit(point.x, 1, int(m_bitmapSrc->Width()) - 2);
	Limit(point.y, 2, int(m_bitmapSrc->Height()) - 3);
	int32 *p = m_backBuf + point.x + point.y * m_bitmapSrc->Width();
	p[0] += (nFlags & ui::MouseLeft) ? 50 : 150;
	p[0] += (nFlags & ui::MouseMiddle) ? 150 : 0;

	int32 w = m_bitmapSrc->Width();
	// Make the initial point of this ripple a bit "fatter".
	p[-1]     += p[0] / 2; p[1]      += p[0] / 2;
	p[-w]     += p[0] / 2; p[w]      += p[0] / 2;
	p[-w - 1] += p[0] / 4; p[-w + 1] += p[0] / 4;
	p[w - 1]  += p[0] / 4; p[w + 1]  += p[0] / 4;

	m_damp = !(nFlags & ui::MouseRight);
	m_activity = true;
}


void CRippleBitmap::OnPaint(ui::Painter &dc)
{
	const Rect rect = GetClientRect();
	ui::Bitmap bitmap(static_cast<int>(m_bitmapTarget->Width()), static_cast<int>(m_bitmapTarget->Height()));
	uint32 *out = bitmap.GetPixels();
	for(const RawImage::Pixel &pixel : m_bitmapTarget->Pixels())
		*out++ = 0xFF000000u | (static_cast<uint32>(pixel.r) << 16) | (static_cast<uint32>(pixel.g) << 8) | pixel.b;
	dc.StretchBitmap(bitmap, rect);
}


bool CRippleBitmap::Animate()
{
	// Were there any pixels being moved in the last frame?
	if(!m_activity)
		return false;

	uint32 now = static_cast<uint32>(Util::GetTickCount64());
	if(now - m_lastFrame < UPDATE_INTERVAL)
		return true;
	m_lastFrame = now;
	m_activity = false;

	m_frontBuf = (m_frame ? m_offset2 : m_offset1).data();
	m_backBuf = (m_frame ? m_offset1 : m_offset2).data();

	// Spread the ripples...
	const int32 w = m_bitmapSrc->Width(), h = m_bitmapSrc->Height();
	const int32 numPixels = w * (h - 2);
	const int32 *back = m_backBuf + w;
	int32 *front = m_frontBuf + w;
	for(int32 i = numPixels; i != 0; i--, back++, front++)
	{
		(*front) = (back[-1] + back[1] + back[w] + back[-w]) / 2 - (*front);
		if(m_damp) (*front) -= (*front) >> 5;
	}

	// ...and compute the final picture.
	const int32 *offset = m_frontBuf + w;
	const RawImage::Pixel *pixelIn = m_bitmapSrc->Pixels().data() + w;
	RawImage::Pixel *pixelOut = m_bitmapTarget->Pixels().data() + w;
	RawImage::Pixel *limitMin = m_bitmapSrc->Pixels().data(), *limitMax = m_bitmapSrc->Pixels().data() + m_bitmapSrc->Pixels().size() - 1;
	for(int32 i = numPixels; i != 0; i--, pixelIn++, pixelOut++, offset++)
	{
		// Compute pixel displacement
		const int32 xOff = offset[-1] - offset[1];
		const int32 yOff = offset[-w] - offset[w];

		if(xOff | yOff)
		{
			const RawImage::Pixel *p = pixelIn + xOff + yOff * w;
			Limit(p, limitMin, limitMax);
			// Add a bit of shading depending on how far we're displacing the pixel...
			pixelOut->r = mpt::saturate_cast<uint8>(p->r + (p->r * xOff) / 32);
			pixelOut->g = mpt::saturate_cast<uint8>(p->g + (p->g * xOff) / 32);
			pixelOut->b = mpt::saturate_cast<uint8>(p->b + (p->b * xOff) / 32);
			// ...and mix it with original picture
			pixelOut->r = (pixelOut->r + pixelIn->r) / 2u;
			pixelOut->g = (pixelOut->g + pixelIn->g) / 2u;
			pixelOut->b = (pixelOut->b + pixelIn->b) / 2u;
			// And now some cheap image smoothing...
			pixelOut[-1].r = (pixelOut->r + pixelOut[-1].r) / 2u;
			pixelOut[-1].g = (pixelOut->g + pixelOut[-1].g) / 2u;
			pixelOut[-1].b = (pixelOut->b + pixelOut[-1].b) / 2u;
			pixelOut[-w].r = (pixelOut->r + pixelOut[-w].r) / 2u;
			pixelOut[-w].g = (pixelOut->g + pixelOut[-w].g) / 2u;
			pixelOut[-w].b = (pixelOut->b + pixelOut[-w].b) / 2u;
			m_activity = true;	// Also use this to update activity status...
		} else
		{
			*pixelOut = *pixelIn;
		}
	}

	m_frame = !m_frame;

	InvalidateRect(NULL, false);

	return true;
}


CAboutDlg::~CAboutDlg()
{
	instance = nullptr;
}


void CAboutDlg::OnOK()
{
	instance = nullptr;
	if(m_TimerID != 0)
	{
		KillTimer(m_TimerID);
		m_TimerID = 0;
	}
	DestroyWindow();
	delete this;
}


void CAboutDlg::OnCancel()
{
	OnOK();
}


static mpt::ustring ArchitectureName()
{
#if MPT_ARCH_AMD64
	return UL_("amd64");
#elif MPT_ARCH_X86
	return UL_("x86");
#elif MPT_ARCH_ARM64
	return UL_("arm64");
#elif MPT_ARCH_ARM
	return UL_("arm");
#else
	return UL_("unknown");
#endif
}


static mpt::ustring GetOperatingSystemName()
{
	std::ifstream osRelease("/etc/os-release");
	std::string line;
	while(std::getline(osRelease, line))
	{
		if(line.starts_with("PRETTY_NAME="))
		{
			line.erase(0, std::string("PRETTY_NAME=").length());
			if(line.size() >= 2 && line.front() == '"' && line.back() == '"')
				line = line.substr(1, line.size() - 2);
			return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, line);
		}
	}
	return UL_("Linux");
}


bool CAboutDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	mpt::ustring app;
	app += MPT_UFORMAT("OpenMPT{} ({}, {} bit)\r\n")(
			mpt::ToUnicode(mpt::Charset::ASCII, OPENMPT_BUILD_VARIANT_MONIKER),
			ArchitectureName(),
			mpt::arch_bits)
		+ UL_("Version ") + Build::GetVersionStringSimple() + UL_("\r\n\r\n")
		+ Build::GetURL(Build::Url::Website) + UL_("\r\n");
	SetDlgItemText(IDC_EDIT3, mpt::ToUnicode(app));

	m_bmp.SubclassDlgItem(IDC_BITMAP1, this);

	m_Tab.InsertItem(0, UL_("OpenMPT"));
	m_Tab.InsertItem(1, UL_("Components"));
	m_Tab.InsertItem(2, UL_("Credits"));
	m_Tab.InsertItem(3, UL_("License"));
	m_Tab.InsertItem(4, UL_("Resources"));
	m_Tab.SetCurSel(0);

	OnTabChange(nullptr, nullptr);

	if(m_TimerID != 0)
	{
		KillTimer(m_TimerID);
		m_TimerID = 0;
	}
	m_TimerID = SetTimer(TIMERID_ABOUT_DEFAULT, CRippleBitmap::UPDATE_INTERVAL);

	return true;
}


void CAboutDlg::OnTimer(uintptr_t nIDEvent)
{
	if(nIDEvent == m_TimerID)
	{
		m_bmp.Animate();
	}
}


void CAboutDlg::OnTabChange(NotifyHeader * /*pNMHDR*/ , LResult * /*pResult*/ )
{
	m_TabEdit.SetWindowText(mpt::ToUnicode(mpt::replace(GetTabText(m_Tab.GetCurSel()), U_("\n"), U_("\r\n"))));
}


#ifdef MPT_ENABLE_ARCH_INTRINSICS
static mpt::ustring CPUFeaturesToString(mpt::arch::current::feature_flags procSupport)
{
	std::vector<mpt::ustring> features;
	MPT_UNUSED_VARIABLE(procSupport);
	return mpt::join_format(features, U_(" "));
}
#endif // MPT_ENABLE_ARCH_INTRINSICS


mpt::ustring CAboutDlg::GetTabText(int tab)
{
	const mpt::ustring lf = UL_("\n");
	const mpt::ustring yes = UL_("yes");
	const mpt::ustring no = UL_("no");
#ifdef MPT_ENABLE_ARCH_INTRINSICS
	const mpt::arch::current::cpu_info CPUInfo = mpt::arch::get_cpu_info();
#endif // MPT_ENABLE_ARCH_INTRINSICS
	mpt::ustring text;
	switch(tab)
	{
		case 0:
			text = UL_("OpenMPT - Open ModPlug Tracker\n\n")
				+ MPT_UFORMAT("Version: {}\n")(Build::GetVersionStringExtended())
				+ MPT_UFORMAT("Source Code: {}\n")(SourceInfo::Current().GetUrlWithRevision() + UL_(" ") + SourceInfo::Current().GetStateString())
				+ MPT_UFORMAT("Build Date: {}\n")(Build::GetBuildDateString())
				+ MPT_UFORMAT("Compiler: {}\n")(Build::GetBuildCompilerString())
				+ MPT_UFORMAT("Architecture: {}\n")(ArchitectureName());
			{
				text += UL_("Required CPU features: ");
				std::vector<mpt::ustring> features;
				#ifdef MPT_ENABLE_ARCH_INTRINSICS
					#if MPT_ARCH_AMD64
						features.push_back(UL_("x86-64"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx) features.push_back(UL_("avx"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx2) features.push_back(UL_("avx2"));
					#elif MPT_ARCH_X86
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::sse) features.push_back(UL_("sse"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::sse2) features.push_back(UL_("sse2"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx) features.push_back(UL_("avx"));
						if(mpt::arch::current::assumed_features() & mpt::arch::current::feature::avx2) features.push_back(UL_("avx2"));
					#endif
				#endif
				text += mpt::join_format(features, U_(" "));
				text += lf;
			}
#ifdef MPT_ENABLE_ARCH_INTRINSICS
			text += MPT_UFORMAT("Optional CPU features used: {}\n")(CPUFeaturesToString(mpt::arch::get_cpu_info().get_features()));
#endif // MPT_ENABLE_ARCH_INTRINSICS
			text += lf;
#ifdef MPT_ENABLE_ARCH_INTRINSICS
#if MPT_ARCH_X86 || MPT_ARCH_AMD64
			text += MPT_UFORMAT("CPU: {}, Family {}, Model {}, Stepping {} ({}/{})\n")
				( mpt::ToUnicode(mpt::Charset::ASCII, (CPUInfo.get_vendor_string().length() > 0) ? CPUInfo.get_vendor_string() : std::string("Generic"))
				, CPUInfo.get_family()
				, CPUInfo.get_model()
				, CPUInfo.get_stepping()
				, mpt::ToUnicode(mpt::Charset::ASCII, CPUInfo.get_vendor_string())
				, mpt::ufmt::hex0<8>(CPUInfo.get_cpuid())
				);
			text += MPT_UFORMAT("CPU Name: {}\n")(mpt::ToUnicode(mpt::Charset::ASCII, (CPUInfo.get_brand_string().length() > 0) ? CPUInfo.get_brand_string() : std::string("")));
#endif
			text += MPT_UFORMAT("Available CPU features: {}\n")(CPUFeaturesToString(CPUInfo.get_features()));
#endif // MPT_ENABLE_ARCH_INTRINSICS
			text += MPT_UFORMAT("Operating System: {}\n\n")(GetOperatingSystemName());
			text += MPT_UFORMAT("OpenMPT Install Path{1}: {0}\n")(theApp.GetInstallPath(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			text += MPT_UFORMAT("OpenMPT Executable Path{1}: {0}\n")(theApp.GetInstallBinArchPath(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			text += MPT_UFORMAT("Settings{1}: {0}\n")(theApp.GetConfigFileName(), theApp.IsPortableMode() ? UV_(" (portable)") : UV_(""));
			break;
		case 1:
			text += UL_("All components are built in.\n");
			break;
		case 2:
			text += Build::GetFullCreditsString();
			break;
		case 3:
			text += Build::GetLicenseString();
			break;
		case 4:
			text += UL_("Website:\n") + Build::GetURL(Build::Url::Website);
			text += UL_("\n\nForum:\n") + Build::GetURL(Build::Url::Forum);
			text += UL_("\n\nBug Tracker:\n") + Build::GetURL(Build::Url::Bugtracker);
			text += UL_("\n\nUpdates:\n") + Build::GetURL(Build::Url::Updates);
			break;
	}
	return text;
}


void CAboutDlg::DoDataExchange(DataExchange* pDX)
{
	DialogBase::DoDataExchange(pDX);
	pDX->BindControl(IDC_TABABOUT, m_Tab);
	pDX->BindControl(IDC_EDITABOUT, m_TabEdit);
}


UI_MESSAGE_MAP_BEGIN(CAboutDlg, DialogBase)
	UI_NOTIFY(ui::TabSelChange, IDC_TABABOUT, &CAboutDlg::OnTabChange)
UI_MESSAGE_MAP_END()



OPENMPT_NAMESPACE_END
