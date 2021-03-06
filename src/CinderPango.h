// CinderPango.h
// PangoBasic
//
// Created by Eric Mika on 1/6/16.
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/gl/gl.h"

#ifdef CINDER_MSW
#include "cinder/cairo/Cairo.h"
#endif

#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>

#include <vector>

namespace kp { namespace pango {

// TODO wrap these up?
const bool grayscale = false;
const bool native = false;

enum class TextAlignment : int {
	LEFT,
	CENTER,
	RIGHT,
	JUSTIFY,
};

enum class TextRenderer {
	FREETYPE,
	PLATFORM_NATIVE,
};

enum class TextWeight : int {
	THIN = 100,
	ULTRALIGHT = 200,
	LIGHT = 300,
	SEMILIGHT = 350,
	BOOK = 380,
	NORMAL = 400,
	MEDIUM = 500,
	SEMIBOLD = 600,
	BOLD = 700,
	ULTRABOLD = 800,
	HEAVY = 900,
	ULTRAHEAVY = 1000
};

enum class TextAntialias : int {
	DEFAULT,
	NONE,
	GRAY,
	SUBPIXEL,
};

using CinderPangoRef = std::shared_ptr<class CinderPango>;

class CinderPango : public std::enable_shared_from_this<CinderPango>
{
public:
	static CinderPangoRef create();
	virtual ~CinderPango();

	// Globals
	static std::vector<std::string> getFontList( bool verbose = false );
	static void logFontList( bool verbose = false );
	static void loadFont( const ci::fs::path &path );
	static TextRenderer getTextRenderer();
	static void setTextRenderer( TextRenderer renderer );

	// Rendering

	const std::string& getText() const;

	// setText can take inline markup to override the default text settings
	// See here for full list of supported tags:
	// https://developer.gnome.org/pango/stable/PangoMarkupFormat.html
	void setText( const std::string &text );

	// Text is rendered into this texture
	ci::gl::TextureRef getTexture() const;

#ifdef CAIRO_HAS_WIN32_SURFACE
	cairo_surface_t* getCairoSurface() const { return pCairoImageSurface; }
#else
	cairo_surface_t* getCairoSurface() const { return pCairoSurface; }
#endif


	// Text smaller than the min size will be clipped
	ci::ivec2 getMinSize() const;
	void setMinSize( int minWidth, int minHeight );
	void setMinSize( const ci::ivec2 &minSize );

	// Text can grow up to this size before a line breaks or clipping begins
	ci::ivec2 getMaxSize() const;
	void setMaxSize( int maxWidth, int maxHeight );
	void setMaxSize( const ci::ivec2 &maxSize );

	// Setting default font styles is more efficient than passing markup via the text string

	void setDefaultTextStyle( const std::string &font = "Sans", 
							  float size = 12.0, 
							  const ci::ColorA &color = ci::Color::black(), 
							  TextWeight weight = TextWeight::NORMAL,
	                          TextAlignment alignment = TextAlignment::LEFT ); // convenience

	const ci::ColorA& getDefaultTextColor() const;
	void setDefaultTextColor( const ci::ColorA &color );

	const ci::ColorA& getBackgroundColor() const;
	void setBackgroundColor( const ci::ColorA &color );

	float getDefaultTextSize() const;
	void setDefaultTextSize( float size );

	const std::string& getDefaultTextFont() const;
	void setDefaultTextFont( const std::string &font );

	TextWeight getDefaultTextWeight() const;
	void setDefaultTextWeight( TextWeight weight );

	TextAntialias getTextAntialias() const;
	void setTextAntialias( TextAntialias mode );

	TextAlignment getTextAlignment() const;
	void setTextAlignment( TextAlignment alignment );

	bool getDefaultTextSmallCapsEnabled() const;
	void setDefaultTextSmallCapsEnabled( bool value );

	bool getDefaultTextItalicsEnabled() const;
	void setDefaultTextItalicsEnabled( bool value );

	float getSpacing() const;
	void setSpacing( float spacing );

	ci::ivec2 getPixelSize() const { return ci::ivec2( mPixelWidth, mPixelHeight ); };

	// Renders text into the texture.
	// Returns true if the texture was actually updated, false if nothing had to change
	// It's reasonable (and more efficient) to just run this in an update loop rather than calling it
	// explicitly after every change to the text state. It will coalesce all invalidations since the
	// last frame and only rebuild what needs to be rebuilt to render the diff.
	// Set force to true to render even if the system thinks state wasn't invalidated.
	bool render( bool force = false );

  protected:
	CinderPango();

  private:
	ci::gl::TextureRef mTexture;
	std::string mText;
	std::string mProcessedText; // stores text after newline filtering
	bool mProbablyHasMarkup;
	ci::ivec2 mMinSize;
	ci::ivec2 mMaxSize;

	// TODO wrap these up...
	std::string mDefaultTextFont;
	bool mDefaultTextItalicsEnabled;
	bool mDefaultTextSmallCapsEnabled;
	ci::ColorA mDefaultTextColor;
	ci::ColorA mBackgroundColor;
	float mDefaultTextSize;
	TextAlignment mTextAlignment;
	TextWeight mDefaultTextWeight;
	TextAntialias mTextAntialias;
	float mSpacing;

	// Internal flags for state invalidation
	// Used by render method
	bool mNeedsFontUpdate;
	bool mNeedsMeasuring;
	bool mNeedsTextRender;
	bool mNeedsFontOptionUpdate;
	bool mNeedsMarkupDetection;

	bool mAutoCreateTexture;

	// simply stored to check for change across renders
	int mPixelWidth;
	int mPixelHeight;

	// Pango references
	PangoFontMap *pFontMap;
	PangoContext *pPangoContext;
	PangoLayout *pPangoLayout;
	PangoFontDescription *pFontDescription;
	cairo_surface_t *pCairoSurface;
	cairo_t *pCairoContext;
	cairo_font_options_t *pCairoFontOptions;

#ifdef CAIRO_HAS_WIN32_SURFACE
	cairo_surface_t *pCairoImageSurface;
#endif
};
}} // namespace kp::pango
