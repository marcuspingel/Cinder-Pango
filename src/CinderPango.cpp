// CinderPango.cpp
// PangoBasic
//
// Created by Eric Mika on 1/6/16.
//

#include "cinder/Log.h"

#include "CinderPango.h"
#include <regex>

#if CAIRO_HAS_WIN32_SURFACE
#include <cairo-win32.h>
#endif

using namespace kp::pango;
using namespace ci;

#pragma mark - Lifecycle

CinderPangoRef CinderPango::create()
{
	return CinderPangoRef( new CinderPango() );
}

CinderPango::CinderPango()
    : mText( "" )
    , mProcessedText( "" )
    , mProbablyHasMarkup( false )
    , mMinSize( ivec2( 0, 0 ) )
    , mMaxSize( ivec2( 320, 240 ) )
    , mDefaultTextFont( "Sans" )
    , mDefaultTextItalicsEnabled( false )
    , mDefaultTextSmallCapsEnabled( false )
    , mDefaultTextColor( ColorA::black() )
    , mBackgroundColor( ColorA::zero() )
    , mDefaultTextSize( 12.0 )
    , mTextAlignment( TextAlignment::LEFT )
    , mDefaultTextWeight( TextWeight::NORMAL )
    , mTextAntialias( TextAntialias::DEFAULT )
    , mSpacing( 0 )
    , mNeedsFontUpdate( false )
    , mNeedsMeasuring( false )
    , mNeedsTextRender( false )
    , mNeedsFontOptionUpdate( false )
    , mNeedsMarkupDetection( false )
    , mPixelWidth( -1 )
    , mPixelHeight( -1 )
{
	// Create Font Map for reuse
	fontMap = nullptr;
	fontMap = pango_cairo_font_map_new();
	if( ! fontMap ) {
		CI_LOG_E("Cannot create the pango font map.");
		return;
	}

	// Create Pango Context for reuse
	pangoContext = nullptr;
	pangoContext = pango_font_map_create_context( fontMap );
	if( ! pangoContext ) {
		CI_LOG_E("Cannot create the pango font context.");
		return;
	}

	// Create Pango Layout for reuse
	pangoLayout = nullptr;
	pangoLayout = pango_layout_new( pangoContext );
	if( ! pangoLayout ) {
		CI_LOG_E("Cannot create the pango layout.");
		return;
	}

	// Initialize Cairo surface and context, will be instantiated on demand
	cairoSurface = nullptr;

#ifdef CAIRO_HAS_WIN32_SURFACE
	cairoImageSurface = nullptr;
#endif

	cairoContext = nullptr;

	// TODO pass these in.....
	// Will be created on demand
	fontDescription = nullptr;

	cairoFontOptions = nullptr;
	cairoFontOptions = cairo_font_options_create();
	if( ! cairoFontOptions ) {
		CI_LOG_E("Cannot create Cairo font options.");
		return;
	}

	// Generate the default font config
	mNeedsFontOptionUpdate = true;
	mNeedsFontUpdate = true;
	render();
}

CinderPango::~CinderPango()
{
	// This causes crash on windows
	if( cairoContext ) {
		cairo_destroy( cairoContext );
	}

	if( fontDescription ) {
		pango_font_description_free( fontDescription );
	}

	if( cairoFontOptions ) {
		cairo_font_options_destroy( cairoFontOptions );
	}

#ifdef CAIRO_HAS_WIN32_SURFACE
	if( cairoImageSurface ) {
		cairo_surface_destroy( cairoImageSurface );
	}
#else
	// Crashes windows...
	if( cairoSurface ) {
		cairo_surface_destroy( cairoSurface );
	}
#endif

	// g_object_unref(pangoContext); // this one crashes Windows?
	g_object_unref( fontMap );
	g_object_unref( pangoLayout );
}

#pragma mark - Getters / Setters

const std::string& CinderPango::getText() const
{
	return mText;
}

void CinderPango::setText( std::string text )
{
	if( text != mText ) {
		mText = text;
		mNeedsMarkupDetection = true;
		mNeedsMeasuring = true;
		mNeedsTextRender = true;
	}
}

gl::TextureRef CinderPango::getTexture() const
{
	// TODO nullptr check?
	return mTexture;
}

void CinderPango::setDefaultTextStyle( std::string font, float size, ColorA color, TextWeight weight, TextAlignment alignment )
{
	this->setDefaultTextFont( font );
	this->setDefaultTextSize( size );
	this->setDefaultTextColor( color );
	this->setDefaultTextWeight( weight );
	this->setTextAlignment( alignment );
}

TextWeight CinderPango::getDefaultTextWeight() const
{
	return mDefaultTextWeight;
}

void CinderPango::setDefaultTextWeight( TextWeight weight )
{
	if( mDefaultTextWeight != weight ) {
		mDefaultTextWeight = weight;
		mNeedsFontUpdate = true;
		mNeedsMeasuring = true;
		mNeedsTextRender = true;
	}
}

TextAlignment CinderPango::getTextAlignment() const
{
	return mTextAlignment;
}

void CinderPango::setTextAlignment( TextAlignment alignment )
{
	if( mTextAlignment != alignment ) {
		mTextAlignment = alignment;
		mNeedsMeasuring = true;
		mNeedsTextRender = true;
	}
}

float CinderPango::getSpacing() const
{
	return mSpacing;
}

void CinderPango::setSpacing( float spacing )
{
	if( mSpacing != spacing ) {
		mSpacing = spacing;
		mNeedsMeasuring = true;
		mNeedsTextRender = true;
	}
}

TextAntialias CinderPango::getTextAntialias() const
{
	return mTextAntialias;
}

void CinderPango::setTextAntialias( TextAntialias mode )
{
	if( mTextAntialias != mode ) {
		mTextAntialias = mode;
		mNeedsFontOptionUpdate = true;
		// TODO does this ever change metrics?
		mNeedsTextRender = true;
	}
}

ivec2 CinderPango::getMinSize() const
{
	return mMinSize;
}

void CinderPango::setMinSize( int minWidth, int minHeight )
{
	setMinSize( ivec2( minWidth, minHeight ) );
}

void CinderPango::setMinSize( ivec2 minSize )
{
	if( mMinSize != minSize ) {
		mMinSize = minSize;
		mNeedsMeasuring = true;
		// Might not need re-rendering
	}
}

ivec2 CinderPango::getMaxSize() const
{
	return mMaxSize;
}

void CinderPango::setMaxSize( int maxWidth, int maxHeight )
{
	setMaxSize( ivec2( maxWidth, maxHeight ) );
}

void CinderPango::setMaxSize( ivec2 maxSize )
{
	if( mMaxSize != maxSize ) {
		mMaxSize = maxSize;
		mNeedsMeasuring = true;
		// Might not need re-rendering
	}
}

ColorA CinderPango::getDefaultTextColor() const
{
	return mDefaultTextColor;
}

void CinderPango::setDefaultTextColor( ColorA color )
{
	if( mDefaultTextColor != color ) {
		mDefaultTextColor = color;
		mNeedsTextRender = true;
	}
}

ColorA CinderPango::getBackgroundColor() const
{
	return mBackgroundColor;
}

void CinderPango::setBackgroundColor( ColorA color )
{
	if( mBackgroundColor != color ) {
		mBackgroundColor = color;
		mNeedsTextRender = true;
	}
}

bool CinderPango::getDefaultTextSmallCapsEnabled() const
{
	return mDefaultTextSmallCapsEnabled;
}

void CinderPango::setDefaultTextSmallCapsEnabled( bool value )
{
	if( mDefaultTextSmallCapsEnabled != value ) {
		mDefaultTextSmallCapsEnabled = value;
		mNeedsFontUpdate = true;
		mNeedsMeasuring = true;
	}
}

bool CinderPango::getDefaultTextItalicsEnabled() const
{
	return mDefaultTextItalicsEnabled;
}

void CinderPango::setDefaultTextItalicsEnabled( bool value )
{
	if( mDefaultTextItalicsEnabled != value ) {
		mDefaultTextItalicsEnabled = value;
		mNeedsFontUpdate = true;
		mNeedsMeasuring = true;
	}
}

float CinderPango::getDefaultTextSize() const
{
	return mDefaultTextSize;
}

void CinderPango::setDefaultTextSize( float size )
{
	if( mDefaultTextSize != size ) {
		mDefaultTextSize = size;
		mNeedsFontUpdate = true;
		mNeedsMeasuring = true;
	}
}

std::string CinderPango::getDefaultTextFont() const
{
	return mDefaultTextFont;
}

void CinderPango::setDefaultTextFont( std::string font )
{
	if( mDefaultTextFont != font ) {
		mDefaultTextFont = font;
		mNeedsFontUpdate = true;
		mNeedsMeasuring = true;
	}
}

#pragma mark - Pango Bridge

bool CinderPango::render( bool force )
{
	if( force || mNeedsFontUpdate || mNeedsMeasuring || mNeedsTextRender || mNeedsMarkupDetection ) {

		// Set options

		if( force || mNeedsMarkupDetection ) {
			// Pango doesn't support HTML-esque line-break tags, so
			// find break marks and replace with newlines, e.g. <br>, <BR>, <br />, <BR />
			std::regex e( "<br\\s?/?>", std::regex_constants::icase );
			mProcessedText = std::regex_replace( mText, e, "\n" );

			// Let's also decide and flag if there's markup in this string
			// Faster to use pango_layout_set_text than pango_layout_set_markup later on if
			// there's no markup to bother with.
			// Be pretty liberal, there's more harm in false-postives than false-negatives
			mProbablyHasMarkup = ( ( mProcessedText.find( "<" ) != std::string::npos ) && ( mProcessedText.find( ">" ) != std::string::npos ) );

			mNeedsMarkupDetection = false;
		}

		// First run, and then if the fonts change

		if( force || mNeedsFontOptionUpdate ) {
			cairo_font_options_set_antialias( cairoFontOptions, static_cast<cairo_antialias_t>( mTextAntialias ) );

			// TODO, expose these?
			cairo_font_options_set_hint_style( cairoFontOptions, CAIRO_HINT_STYLE_FULL );
			cairo_font_options_set_hint_metrics( cairoFontOptions, CAIRO_HINT_METRICS_ON );
			// cairo_font_options_set_subpixel_order(cairoFontOptions, CAIRO_SUBPIXEL_ORDER_DEFAULT);

			pango_cairo_context_set_font_options( pangoContext, cairoFontOptions );

			mNeedsFontOptionUpdate = false;
		}

		if( force || mNeedsFontUpdate ) {
			if( fontDescription != nullptr ) {
				pango_font_description_free( fontDescription );
			}

			fontDescription = pango_font_description_from_string( std::string( mDefaultTextFont + " " + std::to_string( mDefaultTextSize ) ).c_str() );
			pango_font_description_set_weight( fontDescription, static_cast<PangoWeight>( mDefaultTextWeight ) );
			pango_font_description_set_style( fontDescription, mDefaultTextItalicsEnabled ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL );
			pango_font_description_set_variant( fontDescription, mDefaultTextSmallCapsEnabled ? PANGO_VARIANT_SMALL_CAPS : PANGO_VARIANT_NORMAL );
			pango_layout_set_font_description( pangoLayout, fontDescription );
			pango_font_map_load_font( fontMap, pangoContext, fontDescription );

			mNeedsFontUpdate = false;
		}

		bool needsSurfaceResize = false;

		// If the text or the bounds change
		if( force || mNeedsMeasuring ) {

			const int lastPixelWidth = mPixelWidth;
			const int lastPixelHeight = mPixelHeight;

			pango_layout_set_width( pangoLayout, mMaxSize.x * PANGO_SCALE );
			pango_layout_set_height( pangoLayout, mMaxSize.y * PANGO_SCALE );

			// Pango separates alignment and justification... I prefer a simpler API here to handling certain edge cases.
			if( mTextAlignment == TextAlignment::JUSTIFY ) {
				pango_layout_set_justify( pangoLayout, true );
				pango_layout_set_alignment( pangoLayout, static_cast<PangoAlignment>( TextAlignment::LEFT ) );
			} else {
				pango_layout_set_justify( pangoLayout, false );
				pango_layout_set_alignment( pangoLayout, static_cast<PangoAlignment>( mTextAlignment ) );
			}

			// pango_layout_set_wrap(pangoLayout, PANGO_WRAP_CHAR);
			pango_layout_set_spacing( pangoLayout, mSpacing * PANGO_SCALE );

			// TODO set specific attributes...
			// Update font attributes
			// PangoAttrList *attributeList = pango_attr_list_new();
			// PangoAttribute *attribute;
			// attribute = pango_attr_letter_spacing_new(10 * PANGO_SCALE);
			// attribute->start_index = 0;
			// attribute->end_index = -1;
			// pango_attr_list_insert(attributeList, attribute);
			// pango_layout_set_attributes(pangoLayout, attributeList);
			// pango_attr_list_unref(attributeList);

			// Set text, use the fastest method depending on what we found in the text
			if( mProbablyHasMarkup ) {
				pango_layout_set_markup( pangoLayout, mProcessedText.c_str(), -1 );
			} else {
				pango_layout_set_text( pangoLayout, mProcessedText.c_str(), -1 );
			}

			// Measure text
			int newPixelWidth = 0;
			int newPixelHeight = 0;

			pango_layout_get_pixel_size( pangoLayout, &newPixelWidth, &newPixelHeight );

			mPixelWidth = glm::clamp( newPixelWidth, mMinSize.x, mMaxSize.x );
			mPixelHeight = glm::clamp( newPixelHeight, mMinSize.y, mMaxSize.y );

			// Check for change, need to re-render if there's a change
			if( ( mPixelWidth != lastPixelWidth ) || ( mPixelHeight != lastPixelHeight ) ) {
				// Dimensions changed, re-draw text
				needsSurfaceResize = true;
			}

			mNeedsMeasuring = false;
		}

		// Create Cairo surface buffer to draw glyphs into
		// Force this is we need to render but don't have a surface yet
		bool freshCairoSurface = false;

		if( force || needsSurfaceResize || ( mNeedsTextRender && ! cairoSurface ) ) {
			// Create appropriately sized cairo surface
			const bool grayscale = false; // Not really supported
			_cairo_format cairoFormat = grayscale ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_ARGB32;	//	TODO: investigate CAIRO_FORMAT_A8 unreachable on MSW

			// clean up any existing surfaces
			if( cairoSurface != nullptr ) {
				cairo_surface_destroy( cairoSurface );
			}

#if CAIRO_HAS_WIN32_SURFACE
			cairoSurface = cairo_win32_surface_create_with_dib( cairoFormat, mPixelWidth, mPixelHeight );
#else
			cairoSurface = cairo_image_surface_create(cairoFormat, mPixelWidth, mPixelHeight);
#endif

			if( CAIRO_STATUS_SUCCESS != cairo_surface_status( cairoSurface ) ) {
				CI_LOG_E("Error creating Cairo surface.");
				return true;
			}

			// Create context
			/* create our cairo context object that tracks state. */
			if( cairoContext != nullptr ) {
				cairo_destroy( cairoContext );
			}

			cairoContext = cairo_create( cairoSurface );

			if( CAIRO_STATUS_NO_MEMORY == cairo_status( cairoContext ) ) {
				CI_LOG_E("Out of memory, error creating Cairo context");
				return true;
			}

			// Flip vertically
			cairo_scale( cairoContext, 1.0f, -1.0f );
			cairo_translate( cairoContext, 0.0f, -mPixelHeight );
			cairo_move_to( cairoContext, 0, 0 ); // needed?

			mNeedsTextRender = true;
			freshCairoSurface = true;
		}

		if( force || mNeedsTextRender ) {
			// Render text

			if( ( mBackgroundColor == ColorA::zero() ) && ! freshCairoSurface ) {
				// Clear the context... if the background is clear and it's not a brand-new surface buffer
				cairo_save( cairoContext );
				cairo_set_operator( cairoContext, CAIRO_OPERATOR_CLEAR );
				cairo_paint( cairoContext );
				cairo_restore( cairoContext );
			} else {
				// Fill the context with the background color
				cairo_save( cairoContext );
				cairo_set_source_rgba( cairoContext, mBackgroundColor.r, mBackgroundColor.g, mBackgroundColor.b, mBackgroundColor.a );
				cairo_paint( cairoContext );
				cairo_restore( cairoContext );
			}

			// Draw the text into the buffer
			cairo_set_source_rgba( cairoContext, mDefaultTextColor.r, mDefaultTextColor.g, mDefaultTextColor.b, mDefaultTextColor.a );
			pango_cairo_update_layout( cairoContext, pangoLayout );
			pango_cairo_show_layout( cairoContext, pangoLayout );

			// Copy it out to a texture
#ifdef CAIRO_HAS_WIN32_SURFACE
			cairoImageSurface = cairo_win32_surface_get_image( cairoSurface );
			unsigned char *pixels = cairo_image_surface_get_data( cairoImageSurface );
#else
			unsigned char *pixels = cairo_image_surface_get_data(cairoSurface);
#endif

			if( mTexture == nullptr || ( mTexture->getWidth() != mPixelWidth ) || ( mTexture->getHeight() != mPixelHeight ) ) {
				// Create a new texture if needed
				mTexture = gl::Texture2d::create( pixels, GL_BGRA, mPixelWidth, mPixelHeight );
			} else {
				// Update the existing texture
				mTexture->update( pixels, GL_BGRA, GL_UNSIGNED_BYTE, 0, mPixelWidth, mPixelHeight );
			}

			mNeedsTextRender = false;
		}

		return true;
	} else {
		return false;
	}
}

#pragma mark - Static Methods

void CinderPango::setTextRenderer( kp::pango::TextRenderer renderer )
{
	std::string rendererName = "";

	switch( renderer ) {
		case TextRenderer::PLATFORM_NATIVE:
#if defined(CINDER_MSW)
			rendererName = "win32";
#elif defined(CINDER_MAC)
			rendererName = "coretext";
#else
			CI_LOG_E("Setting Pango text renderer not supported on this platform.");
#endif
			break;
		case TextRenderer::FREETYPE:
		{
			rendererName = "fontconfig";
		}
			break;
	}

	if( rendererName != "" ) {
#ifdef CINDER_MSW
		int status = _putenv_s( "PANGOCAIRO_BACKEND", rendererName.c_str() );
#else
		int status = setenv("PANGOCAIRO_BACKEND", rendererName.c_str(), 1); // this fixes some font issues on  mac
#endif
		if( status == 0 ) {
			CI_LOG_V("Set Pango Cairo backend renderer to: " << rendererName);
		} else {
			CI_LOG_E("Error setting Pango Cairo backend environment variable.");
		}
	}
}

TextRenderer CinderPango::getTextRenderer()
{
	const char *rendererName = std::getenv( "PANGOCAIRO_BACKEND" );

	if( rendererName == nullptr ) {
		CI_LOG_E("Could not read Pango Cairo backend environment variable. Assuming native renderer.");
		return TextRenderer::PLATFORM_NATIVE;
	}

	std::string rendererNameString( rendererName );

	if( ( rendererNameString == "win32" ) || ( rendererNameString == "coretext" ) ) {
		return TextRenderer::PLATFORM_NATIVE;
	}

	if( ( rendererNameString == "fontconfig" ) || ( rendererNameString == "fc" ) ) {
		return TextRenderer::FREETYPE;
	}

	CI_LOG_E("Unknown Pango Cairo backend environment variable: " << rendererNameString << ". Assuming native renderer.");
	return TextRenderer::PLATFORM_NATIVE;
}

void CinderPango::loadFont( const fs::path &path )
{
	const FcChar8 *fcPath = reinterpret_cast<const FcChar8 * >( path.c_str() );
	FcBool fontAddStatus = FcConfigAppFontAddFile( FcConfigGetCurrent(), fcPath );

	if( !fontAddStatus ) {
		CI_LOG_E("Pango failed to load font from file \"" << path << "\"");
	} else {
		CI_LOG_V("Pango thinks it loaded font " << path << " with status " << fontAddStatus);
	}
}

std::vector<std::string> CinderPango::getFontList( bool verbose )
{
	std::vector<std::string> fontList;

	// http: // www.lemoda.net/pango/list-fonts/
	// https://code.google.com/p/serif/source/browse/fontview/trunk/src/font-model.c
	int i;
	PangoFontFamily **families;
	int n_families;
	PangoFontMap *fontmap;

	fontmap = pango_cairo_font_map_get_default();
	pango_font_map_list_families( fontmap, &families, &n_families );
	// printf("There are %d families\n", n_families);
	for( i = 0; i < n_families; i++ ) {
		PangoFontFamily *family = families[ i ];

		const char *family_name;
		family_name = pango_font_family_get_name( family );
		fontList.push_back( family_name );

		if( verbose ) {
			CI_LOG_V("Family " << i << ": " << family_name);

			// Also interrogate individual fonts in the family
			// Useful if something isn't rendering correctly
			PangoFontFace **pFontFaces = nullptr;
			int numFontFaces = 0;
			pango_font_family_list_faces( family, &pFontFaces, &numFontFaces );

			// Get a description of each weight
			for( int j = 0; j < numFontFaces; j++ ) {
				PangoFontFace *face = pFontFaces[ j ];

				const char *face_name = pango_font_face_get_face_name( face );
				PangoFontDescription *description = pango_font_face_describe( face );
				const char *description_string = pango_font_description_to_string( description );
				PangoWeight weight = pango_font_description_get_weight( description );
				uint32_t hash = pango_font_description_hash( description );

				CI_LOG_V("\tFace " << j << ": " << face_name);
				CI_LOG_V("\t\tDescription: " << description_string);
				CI_LOG_V("\t\tWeight: " << weight);
				CI_LOG_V("\t\tHash: " << hash);
				// TODO more stuff?

				pango_font_description_free( description );
			}

			g_free( pFontFaces );
		}
	}
	g_free( families );

	return fontList;
}

void CinderPango::logFontList( bool verbose )
{
	auto fontList = getFontList( verbose );

	int index = 0;
	for( auto &fontName : fontList ) {
		CI_LOG_V("Font " << index << ": " << fontName);
		index++;
	}
}
