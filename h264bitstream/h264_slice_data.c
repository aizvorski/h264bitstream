/* 
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2006 Auroras Entertainment, LLC
 * 
 * Written by Alex Izvorski <aizvorski@gmail.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


/******* data *******/

//TODO determine the size of all arrays
typedef struct
{
    int mb_type;

    // pcm mb only
    int pcm_sample_luma[];
    int pcm_sample_chroma[];

    int transform_size_8x8_flag;
    int mb_qp_delta;
    int mb_field_decoding_flag;
    int mb_skip_flag;

    // intra mb only
    int prev_intra4x4_pred_mode_flag[];
    int rem_intra4x4_pred_mode[];
    int prev_intra8x8_pred_mode_flag[];
    int rem_intra8x8_pred_mode[];
    int intra_chroma_pred_mode;

    // inter mb only
    int ref_idx_l0[];
    int ref_idx_l1[];
    int mvd_l0[][][];
    int mvd_l1[][][];

    // residuals
    int coded_block_pattern;

    int Intra16x16DCLevel[16];
    int Intra16x16ACLevel[][15];
    int LumaLevel[][16];
    int LumaLevel8x8[][64];
    int ChromaDCLevel[2][];
    int ChromaACLevel[2][][15];

} macroblock_t;


typedef struct 
{
    macroblock_t* mbs;
} slice_t;


/****** bitstream functions - not already implemented ******/

uint32_t bs_read_te(bs_t* b);
void bs_write_te(bs_t* b, uint32_t v);

// CABAC
// 9.3 CABAC parsing process for slice data
// NOTE: these functions will need more arguments, since how they work depends on *what* is being encoded/decoded
// for now, just a placeholder for places that we will need to call this from
uint32_t bs_read_ae(bs_t* b);
void bs_write_ae(bs_t* b, uint32_t v);

// CALVC
// 9.2 CAVLC parsing process for transform coefficient levels
uint32_t bs_read_ce(bs_t* b);
void bs_write_ce(bs_t* b, uint32_t v);


/****** reading ******/


//7.3.4 Slice data syntax
void read_slice_data( h264_stream_t* h, bs_t* b )
{
    if( entropy_coding_mode_flag )
    {
        while( !byte_aligned( ) )
        {
            cabac_alignment_one_bit = bs_read_f(b, 1);
        }
    }
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do
    {
        if( slice_type != I && slice_type != SI )
        {
            if( !entropy_coding_mode_flag )
            {
                mb_skip_run = bs_read_ue(b);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                {
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                }
                moreDataFlag = more_rbsp_data( );
            }
            else
            {
                mb_skip_flag = bs_read_ae(b);
                moreDataFlag = !mb_skip_flag;
            }
        }
        if( moreDataFlag )
        {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
            {
                if( !cabac ) { mb_field_decoding_flag = bs_read_u(b, 1); }
                else         { mb_field_decoding_flag = bs_read_ae(b); }
            }
            read_macroblock_layer( h, b );
        }
        if( !entropy_coding_mode_flag )
        {
            moreDataFlag = more_rbsp_data( );
        }
        else
        {
            if( slice_type != I && slice_type != SI )
            {
                prevMbSkipped = mb_skip_flag;
            }
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
            {
                moreDataFlag = 1;
            }
            else
            {
                end_of_slice_flag = bs_read_ae(b);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}


//7.3.5 Macroblock layer syntax
void read_macroblock_layer( h264_stream_t* h, bs_t* b )
{
    if( !cabac ) { mb_type = bs_read_ue(b); }
    else         { mb_type = bs_read_ae(b); }
    if( mb_type == I_PCM )
    {
        while( !byte_aligned( ) )
        {
            pcm_alignment_zero_bit = bs_read_f(b, 1);
        }
        for( i = 0; i < 256; i++ )
        {
            pcm_sample_luma[ i ] = bs_read_u(b);
        }
        for( i = 0; i < 2 * MbWidthC * MbHeightC; i++ )
        {
            pcm_sample_chroma[ i ] = bs_read_u(b);
        }
    }
    else
    {
        noSubMbPartSizeLessThan8x8Flag = 1;
        if( mb_type != I_NxN &&
            MbPartPredMode( mb_type, 0 ) != Intra_16x16 &&
            NumMbPart( mb_type ) == 4 )
        {
            read_sub_mb_pred( h, b, mb_type );
            for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
            {
                if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 )
                {
                    if( NumSubMbPart( sub_mb_type[ mbPartIdx ] ) > 1 )
                    {
                        noSubMbPartSizeLessThan8x8Flag = 0;
                    }
                }
                else if( !direct_8x8_inference_flag )
                {
                    noSubMbPartSizeLessThan8x8Flag = 0;
                }
            }
        }
        else
        {
            if( transform_8x8_mode_flag && mb_type == I_NxN )
            {
                if( !cabac ) { transform_size_8x8_flag = bs_read_u(b, 1); }
                else         { transform_size_8x8_flag = bs_read_ae(b); }
            }
            read_mb_pred( h, b, mb_type );
        }
        if( MbPartPredMode( mb_type, 0 ) != Intra_16x16 )
        {
            if( !cabac ) { coded_block_pattern = bs_read_me(b); }
            else         { coded_block_pattern = bs_read_ae(b); }
            if( CodedBlockPatternLuma > 0 &&
                transform_8x8_mode_flag && mb_type != I_NxN &&
                noSubMbPartSizeLessThan8x8Flag &&
                ( mb_type != B_Direct_16x16 || direct_8x8_inference_flag ) )
            {
                if( !cabac ) { transform_size_8x8_flag = bs_read_u(b, 1); }
                else         { transform_size_8x8_flag = bs_read_ae(b); }
            }
        }
        if( CodedBlockPatternLuma > 0 || CodedBlockPatternChroma > 0 ||
            MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
        {
            if( !cabac ) { mb_qp_delta = bs_read_se(b); }
            else         { mb_qp_delta = bs_read_ae(b); }
            read_residual( h, b );
        }
    }
}

//7.3.5.1 Macroblock prediction syntax
void read_mb_pred( h264_stream_t* h, bs_t* b, int mb_type )
{
    if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ||
        MbPartPredMode( mb_type, 0 ) == Intra_8x8 ||
        MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
    {
        if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 )
        {
            for( luma4x4BlkIdx=0; luma4x4BlkIdx<16; luma4x4BlkIdx++ )
            {
                if( !cabac ) { prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] = bs_read_u(b, 1); }
                else         { prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] = bs_read_ae(b); }
                if( !prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] )
                {
                    if( !cabac ) { rem_intra4x4_pred_mode[ luma4x4BlkIdx ] = bs_read_u(b, 3); }
                    else         { rem_intra4x4_pred_mode[ luma4x4BlkIdx ] = bs_read_ae(b); }
                }
            }
        }
        if( MbPartPredMode( mb_type, 0 ) == Intra_8x8 )
        {
            for( luma8x8BlkIdx=0; luma8x8BlkIdx<4; luma8x8BlkIdx++ )
            {
                if( !cabac ) { prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ] = bs_read_u(b, 1); }
                else         { prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ] = bs_read_ae(b); }
                if( !prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ] )
                {
                    if( !cabac ) { rem_intra8x8_pred_mode[ luma8x8BlkIdx ] = bs_read_u(b, 3); }
                    else         { rem_intra8x8_pred_mode[ luma8x8BlkIdx ] = bs_read_ae(b); }
                }
            }
        }
        if( chroma_format_idc != 0 )
        {
            if( !cabac ) { intra_chroma_pred_mode = bs_read_ue(b); }
            else         { intra_chroma_pred_mode = bs_read_ae(b); }
        }
    }
    else if( MbPartPredMode( mb_type, 0 ) != Direct )
    {
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( ( num_ref_idx_l0_active_minus1 > 0 ||
                  mb_field_decoding_flag ) &&
                MbPartPredMode( mb_type, mbPartIdx ) != Pred_L1 )
            {
                if( !cabac ) { ref_idx_l0[ mbPartIdx ] = bs_read_te(b); }
                else         { ref_idx_l0[ mbPartIdx ] = bs_read_ae(b); }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( ( num_ref_idx_l1_active_minus1 > 0 ||
                  mb_field_decoding_flag ) &&
                MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 )
            {
                if( !cabac ) { ref_idx_l1[ mbPartIdx ] = bs_read_te(b); }
                else         { ref_idx_l1[ mbPartIdx ] = bs_read_ae(b); }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( MbPartPredMode ( mb_type, mbPartIdx ) != Pred_L1 )
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { mvd_l0[ mbPartIdx ][ 0 ][ compIdx ] = bs_read_se(b); }
                    else         { mvd_l0[ mbPartIdx ][ 0 ][ compIdx ] = bs_read_ae(b); }
                }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 )
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { mvd_l1[ mbPartIdx ][ 0 ][ compIdx ] = bs_read_se(b); }
                    else         { mvd_l1[ mbPartIdx ][ 0 ][ compIdx ] = bs_read_ae(b); }
                }
            }
        }
    }
}

//7.3.5.2  Sub-macroblock prediction syntax
void read_sub_mb_pred( h264_stream_t* h, bs_t* b, int mb_type )
{
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( !cabac ) { sub_mb_type[ mbPartIdx ] = bs_read_ue(b); }
        else         { sub_mb_type[ mbPartIdx ] = bs_read_ae(b); }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( ( num_ref_idx_l0_active_minus1 > 0 || mb_field_decoding_flag ) &&
            mb_type != P_8x8ref0 &&
            sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 )
        {
            if( !cabac ) { ref_idx_l0[ mbPartIdx ] = bs_read_te(b); }
            else         { ref_idx_l0[ mbPartIdx ] = bs_read_ae(b); }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( (num_ref_idx_l1_active_minus1 > 0 || mb_field_decoding_flag ) &&
            sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 )
        {
            if( !cabac ) { ref_idx_l1[ mbPartIdx ] = bs_read_te(b); }
            else         { ref_idx_l1[ mbPartIdx ] = bs_read_ae(b); }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 )
        {
            for( subMbPartIdx = 0;
                 subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );
                 subMbPartIdx++)
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { mvd_l0[ mbPartIdx ][ subMbPartIdx ][ compIdx ] = bs_read_se(b); }
                    else         { mvd_l0[ mbPartIdx ][ subMbPartIdx ][ compIdx ] = bs_read_ae(b); }
                }
            }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 )
        {
            for( subMbPartIdx = 0;
                 subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );
                 subMbPartIdx++)
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { mvd_l1[ mbPartIdx ][ subMbPartIdx ][ compIdx ] = bs_read_se(b); }
                    else         { mvd_l1[ mbPartIdx ][ subMbPartIdx ][ compIdx ] = bs_read_ae(b); }
                }
            }
        }
    }
}

//7.3.5.3 Residual data syntax
void read_residual( h264_stream_t* h, bs_t* b )
{
    if( !entropy_coding_mode_flag )
    {
        residual_block = residual_block_cavlc;
    }
    else
    {
        residual_block = residual_block_cabac;
    }
    if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
    {
        residual_block( Intra16x16DCLevel, 16 );
    }
    for( i8x8 = 0; i8x8 < 4; i8x8++ ) // each luma 8x8 block
    {
        if( !transform_size_8x8_flag || !entropy_coding_mode_flag )
        {
            for( i4x4 = 0; i4x4 < 4; i4x4++ ) // each 4x4 sub-block of block
            {
                if( CodedBlockPatternLuma & ( 1 << i8x8 ) )
                {
                    if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
                    {
                        residual_block( Intra16x16ACLevel[ i8x8 * 4 + i4x4 ], 15 );
                    }
                    else
                    {
                        residual_block( LumaLevel[ i8x8 * 4 + i4x4 ], 16 );
                    }
                }
                else if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
                {
                    for( i = 0; i < 15; i++ )
                    {
                        Intra16x16ACLevel[ i8x8 * 4 + i4x4 ][ i ] = 0;
                    }
                }
                else
                {
                    for( i = 0; i < 16; i++ )
                    {
                        LumaLevel[ i8x8 * 4 + i4x4 ][ i ] = 0;
                    }
                }
                if( !entropy_coding_mode_flag && transform_size_8x8_flag )
                {
                    for( i = 0; i < 16; i++ )
                    {
                        LumaLevel8x8[ i8x8 ][ 4 * i + i4x4 ] = LumaLevel[ i8x8 * 4 + i4x4 ][ i ];
                    }
                }
            }
        }
        else if( CodedBlockPatternLuma & ( 1 << i8x8 ) )
        {
            residual_block( LumaLevel8x8[ i8x8 ], 64 );
        }
        else
        {
            for( i = 0; i < 64; i++ )
            {
                LumaLevel8x8[ i8x8 ][ i ] = 0;
            }
        }
    }
    if( chroma_format_idc != 0 )
    {
        NumC8x8 = 4 / ( SubWidthC * SubHeightC );
        for( iCbCr = 0; iCbCr < 2; iCbCr++ )
        {
            if( CodedBlockPatternChroma & 3 ) // chroma DC residual present
            {
                residual_block( ChromaDCLevel[ iCbCr ], 4 * NumC8x8 );
            }
            else
            {
                for( i = 0; i < 4 * NumC8x8; i++ )
                {
                    ChromaDCLevel[ iCbCr ][ i ] = 0;
                }
            }
        }
        for( iCbCr = 0; iCbCr < 2; iCbCr++ )
        {
            for( i8x8 = 0; i8x8 < NumC8x8; i8x8++ )
            {
                for( i4x4 = 0; i4x4 < 4; i4x4++ )
                {
                    if( CodedBlockPatternChroma & 2 )  // chroma AC residual present
                    {
                        residual_block( ChromaACLevel[ iCbCr ][ i8x8*4+i4x4 ], 15);
                    }
                    else
                    {
                        for( i = 0; i < 15; i++ )
                        {
                            ChromaACLevel[ iCbCr ][ i8x8*4+i4x4 ][ i ] = 0;
                        }
                    }
                }
            }
        }
    }

}


//7.3.5.3.1 Residual block CAVLC syntax
void read_residual_block_cavlc( bs_t* b, int* coeffLevel, int maxNumCoeff )
{
    for( i = 0; i < maxNumCoeff; i++ )
    {
        coeffLevel[ i ] = 0;
    }
    coeff_token = bs_read_ce(b);
    if( TotalCoeff( coeff_token ) > 0 )
    {
        if( TotalCoeff( coeff_token ) > 10 && TrailingOnes( coeff_token ) < 3 )
        {
            suffixLength = 1;
        }
        else
        {
            suffixLength = 0;
        }
        for( i = 0; i < TotalCoeff( coeff_token ); i++ )
        {
            if( i < TrailingOnes( coeff_token ) )
            {
                trailing_ones_sign_flag = bs_read_u(b, 1);
                level[ i ] = 1 - 2 * trailing_ones_sign_flag;
            }
            else
            {
                level_prefix = bs_read_ce(b);
                levelCode = ( Min( 15, level_prefix ) << suffixLength );
                if( suffixLength > 0 || level_prefix >= 14 )
                {
                    level_suffix = bs_read_u(b);
                    levelCode += level_suffix;
                }
                if( level_prefix > = 15 && suffixLength == 0 )
                {
                    levelCode += 15;
                }
                if( level_prefix > = 16 )
                {
                    levelCode += ( 1 << ( level_prefix - 3 ) ) - 4096;
                }
                if( i == TrailingOnes( coeff_token ) &&
                    TrailingOnes( coeff_token ) < 3 )
                {
                    levelCode += 2;
                }
                if( levelCode % 2 == 0 )
                {
                    level[ i ] = ( levelCode + 2 ) >> 1;
                }
                else
                {
                    level[ i ] = ( -levelCode - 1 ) >> 1;
                }
                if( suffixLength == 0 )
                {
                    suffixLength = 1;
                }
                if( Abs( level[ i ] ) > ( 3 << ( suffixLength - 1 ) ) &&
                    suffixLength < 6 )
                {
                    suffixLength++;
                }
            }
        }
        if( TotalCoeff( coeff_token ) < maxNumCoeff )
        {
            total_zeros = bs_read_ce(b);
            zerosLeft = total_zeros;
        } else
        {
            zerosLeft = 0;
        }
        for( i = 0; i < TotalCoeff( coeff_token ) - 1; i++ )
        {
            if( zerosLeft > 0 )
            {
                run_before = bs_read_ce(b);
                run[ i ] = run_before;
            } else
            {
                run[ i ] = 0;
            }
            zerosLeft = zerosLeft - run[ i ];
        }
        run[ TotalCoeff( coeff_token ) - 1 ] = zerosLeft;
        coeffNum = -1;

        for( i = TotalCoeff( coeff_token ) - 1; i >= 0; i-- )
        {
            coeffNum += run[ i ] + 1;
            coeffLevel[ coeffNum ] = level[ i ];
        }
    }
}



//7.3.5.3.2 Residual block CABAC syntax
void read_residual_block_cabac( bs_t* b, int* coeffLevel, int maxNumCoeff )
{
    if( maxNumCoeff == 64 )
    {
        coded_block_flag = 1;
    }
    else
    {
        coded_block_flag = bs_read_ae(b);
    }
    if( coded_block_flag )
    {
        numCoeff = maxNumCoeff;
        i=0;
        do
        {
            significant_coeff_flag[ i ] = bs_read_ae(b);
            if( significant_coeff_flag[ i ] )
            {
                last_significant_coeff_flag[ i ] = bs_read_ae(b);
                if( last_significant_coeff_flag[ i ] )
                {
                    numCoeff = i + 1;
                    for( j = numCoeff; j < maxNumCoeff; j++ )
                    {
                        coeffLevel[ j ] = 0;
                    }
                }
            }
            i++;
        } while( i < numCoeff - 1 );

        coeff_abs_level_minus1[ numCoeff - 1 ] = bs_read_ae(b);
        coeff_sign_flag[ numCoeff - 1 ] = bs_read_ae(b);
        coeffLevel[ numCoeff - 1 ] =
            ( coeff_abs_level_minus1[ numCoeff - 1 ] + 1 ) *
            ( 1 - 2 * coeff_sign_flag[ numCoeff - 1 ] );
        for( i = numCoeff - 2; i >= 0; i-- )
        {
            if( significant_coeff_flag[ i ] )
            {
                coeff_abs_level_minus1[ i ] = bs_read_ae(b);
                coeff_sign_flag[ i ] = bs_read_ae(b);
                coeffLevel[ i ] = ( coeff_abs_level_minus1[ i ] + 1 ) *
                    ( 1 - 2 * coeff_sign_flag[ i ] );
            }
            else
            {
                coeffLevel[ i ] = 0;
            }
        }
    }
    else
    {
        for( i = 0; i < maxNumCoeff; i++ )
        {
            coeffLevel[ i ] = 0;
        }
    }
}


/****** writing ******/

//7.3.4 Slice data syntax
write_slice_data( )
{
    if( entropy_coding_mode_flag )
    {
        while( !byte_aligned( ) )
        {
            bs_write_f(b, 1, cabac_alignment_one_bit);
        }
    }
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do
    {
        if( slice_type != I && slice_type != SI )
        {
            if( !entropy_coding_mode_flag )
            {
                bs_write_ue(b, mb_skip_run);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                {
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                }
                moreDataFlag = more_rbsp_data( );
            }
            else
            {
                bs_write_ae(b, mb_skip_flag);
                moreDataFlag = !mb_skip_flag;
            }
        }
        if( moreDataFlag )
        {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
            {
                if( !cabac ) { bs_write_u(b, 1, mb_field_decoding_flag); }
                else         { bs_write_ae(b, mb_field_decoding_flag); }
            }
            macroblock_layer( );
        }
        if( !entropy_coding_mode_flag )
        {
            moreDataFlag = more_rbsp_data( );
        }
        else
        {
            if( slice_type != I && slice_type != SI )
            {
                prevMbSkipped = mb_skip_flag;
            }
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
            {
                moreDataFlag = 1;
            }
            else
            {
                bs_write_ae(b, end_of_slice_flag);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}


//7.3.5 Macroblock layer syntax
write_macroblock_layer( )
{
    if( !cabac ) { bs_write_ue(b, mb_type); }
    else         { bs_write_ae(b, mb_type); }
    if( mb_type == I_PCM )
    {
        while( !byte_aligned( ) )
        {
            bs_write_f(b, 1, pcm_alignment_zero_bit);
        }
        for( i = 0; i < 256; i++ )
        {
            bs_write_u(b, pcm_sample_luma[ i ]);
        }
        for( i = 0; i < 2 * MbWidthC * MbHeightC; i++ )
        {
            bs_write_u(b, pcm_sample_chroma[ i ]);
        }
    }
    else
    {
        noSubMbPartSizeLessThan8x8Flag = 1;
        if( mb_type != I_NxN &&
            MbPartPredMode( mb_type, 0 ) != Intra_16x16 &&
            NumMbPart( mb_type ) == 4 )
        {
            sub_mb_pred( mb_type );
            for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
            {
                if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 )
                {
                    if( NumSubMbPart( sub_mb_type[ mbPartIdx ] ) > 1 )
                    {
                        noSubMbPartSizeLessThan8x8Flag = 0;
                    }
                }
                else if( !direct_8x8_inference_flag )
                {
                    noSubMbPartSizeLessThan8x8Flag = 0;
                }
            }
        }
        else
        {
            if( transform_8x8_mode_flag && mb_type == I_NxN )
            {
                if( !cabac ) { bs_write_u(b, 1, transform_size_8x8_flag); }
                else         { bs_write_ae(b, transform_size_8x8_flag); }
            }
            mb_pred( mb_type );
        }
        if( MbPartPredMode( mb_type, 0 ) != Intra_16x16 )
        {
            if( !cabac ) { bs_write_me(b, coded_block_pattern); }
            else         { bs_write_ae(b, coded_block_pattern); }
            if( CodedBlockPatternLuma > 0 &&
                transform_8x8_mode_flag && mb_type != I_NxN &&
                noSubMbPartSizeLessThan8x8Flag &&
                ( mb_type != B_Direct_16x16 || direct_8x8_inference_flag ) )
            {
                if( !cabac ) { bs_write_u(b, 1, transform_size_8x8_flag); }
                else         { bs_write_ae(b, transform_size_8x8_flag); }
            }
        }
        if( CodedBlockPatternLuma > 0 || CodedBlockPatternChroma > 0 ||
            MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
        {
            if( !cabac ) { bs_write_se(b, mb_qp_delta); }
            else         { bs_write_ae(b, mb_qp_delta); }
            residual( );
        }
    }
}

//7.3.5.1 Macroblock prediction syntax
write_mb_pred( mb_type )
{
    if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 ||
        MbPartPredMode( mb_type, 0 ) == Intra_8x8 ||
        MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
    {
        if( MbPartPredMode( mb_type, 0 ) == Intra_4x4 )
        {
            for( luma4x4BlkIdx=0; luma4x4BlkIdx<16; luma4x4BlkIdx++ )
            {
                if( !cabac ) { bs_write_u(b, 1, prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ]); }
                else         { bs_write_ae(b, prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ]); }
                if( !prev_intra4x4_pred_mode_flag[ luma4x4BlkIdx ] )
                {
                    if( !cabac ) { bs_write_u(b, 3, rem_intra4x4_pred_mode[ luma4x4BlkIdx ]); }
                    else         { bs_write_ae(b, rem_intra4x4_pred_mode[ luma4x4BlkIdx ]); }
                }
            }
        }
        if( MbPartPredMode( mb_type, 0 ) == Intra_8x8 )
        {
            for( luma8x8BlkIdx=0; luma8x8BlkIdx<4; luma8x8BlkIdx++ )
            {
                if( !cabac ) { bs_write_u(b, 1, prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ]); }
                else         { bs_write_ae(b, prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ]); }
                if( !prev_intra8x8_pred_mode_flag[ luma8x8BlkIdx ] )
                {
                    if( !cabac ) { bs_write_u(b, 3, rem_intra8x8_pred_mode[ luma8x8BlkIdx ]); }
                    else         { bs_write_ae(b, rem_intra8x8_pred_mode[ luma8x8BlkIdx ]); }
                }
            }
        }
        if( chroma_format_idc != 0 )
        {
            if( !cabac ) { bs_write_ue(b, intra_chroma_pred_mode); }
            else         { bs_write_ae(b, intra_chroma_pred_mode); }
        }
    }
    else if( MbPartPredMode( mb_type, 0 ) != Direct )
    {
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( ( num_ref_idx_l0_active_minus1 > 0 ||
                  mb_field_decoding_flag ) &&
                MbPartPredMode( mb_type, mbPartIdx ) != Pred_L1 )
            {
                if( !cabac ) { bs_write_te(b, ref_idx_l0[ mbPartIdx ]); }
                else         { bs_write_ae(b, ref_idx_l0[ mbPartIdx ]); }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( ( num_ref_idx_l1_active_minus1 > 0 ||
                  mb_field_decoding_flag ) &&
                MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 )
            {
                if( !cabac ) { bs_write_te(b, ref_idx_l1[ mbPartIdx ]); }
                else         { bs_write_ae(b, ref_idx_l1[ mbPartIdx ]); }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( MbPartPredMode ( mb_type, mbPartIdx ) != Pred_L1 )
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { bs_write_se(b, mvd_l0[ mbPartIdx ][ 0 ][ compIdx ]); }
                    else         { bs_write_ae(b, mvd_l0[ mbPartIdx ][ 0 ][ compIdx ]); }
                }
            }
        }
        for( mbPartIdx = 0; mbPartIdx < NumMbPart( mb_type ); mbPartIdx++)
        {
            if( MbPartPredMode( mb_type, mbPartIdx ) != Pred_L0 )
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { bs_write_se(b, mvd_l1[ mbPartIdx ][ 0 ][ compIdx ]); }
                    else         { bs_write_ae(b, mvd_l1[ mbPartIdx ][ 0 ][ compIdx ]); }
                }
            }
        }
    }
}

//7.3.5.2  Sub-macroblock prediction syntax
write_sub_mb_pred( mb_type )
{
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( !cabac ) { bs_write_ue(b, sub_mb_type[ mbPartIdx ]); }
        else         { bs_write_ae(b, sub_mb_type[ mbPartIdx ]); }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( ( num_ref_idx_l0_active_minus1 > 0 || mb_field_decoding_flag ) &&
            mb_type != P_8x8ref0 &&
            sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 )
        {
            if( !cabac ) { bs_write_te(b, ref_idx_l0[ mbPartIdx ]); }
            else         { bs_write_ae(b, ref_idx_l0[ mbPartIdx ]); }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( (num_ref_idx_l1_active_minus1 > 0 || mb_field_decoding_flag ) &&
            sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 )
        {
            if( !cabac ) { bs_write_te(b, ref_idx_l1[ mbPartIdx ]); }
            else         { bs_write_ae(b, ref_idx_l1[ mbPartIdx ]); }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L1 )
        {
            for( subMbPartIdx = 0;
                 subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );
                 subMbPartIdx++)
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { bs_write_se(b, mvd_l0[ mbPartIdx ][ subMbPartIdx ][ compIdx ]); }
                    else         { bs_write_ae(b, mvd_l0[ mbPartIdx ][ subMbPartIdx ][ compIdx ]); }
                }
            }
        }
    }
    for( mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++ )
    {
        if( sub_mb_type[ mbPartIdx ] != B_Direct_8x8 &&
            SubMbPredMode( sub_mb_type[ mbPartIdx ] ) != Pred_L0 )
        {
            for( subMbPartIdx = 0;
                 subMbPartIdx < NumSubMbPart( sub_mb_type[ mbPartIdx ] );
                 subMbPartIdx++)
            {
                for( compIdx = 0; compIdx < 2; compIdx++ )
                {
                    if( !cabac ) { bs_write_se(b, mvd_l1[ mbPartIdx ][ subMbPartIdx ][ compIdx ]); }
                    else         { bs_write_ae(b, mvd_l1[ mbPartIdx ][ subMbPartIdx ][ compIdx ]); }
                }
            }
        }
    }
}

//7.3.5.3 Residual data syntax
write_residual( )
{
    if( !entropy_coding_mode_flag )
    {
        residual_block = residual_block_cavlc;
    }
    else
    {
        residual_block = residual_block_cabac;
    }
    if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
    {
        residual_block( Intra16x16DCLevel, 16 );
    }
    for( i8x8 = 0; i8x8 < 4; i8x8++ ) // each luma 8x8 block
    {
        if( !transform_size_8x8_flag || !entropy_coding_mode_flag )
        {
            for( i4x4 = 0; i4x4 < 4; i4x4++ ) // each 4x4 sub-block of block
            {
                if( CodedBlockPatternLuma & ( 1 << i8x8 ) )
                {
                    if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
                    {
                        residual_block( Intra16x16ACLevel[ i8x8 * 4 + i4x4 ], 15 );
                    }
                    else
                    {
                        residual_block( LumaLevel[ i8x8 * 4 + i4x4 ], 16 );
                    }
                }
                else if( MbPartPredMode( mb_type, 0 ) == Intra_16x16 )
                {
                    for( i = 0; i < 15; i++ )
                    {
                        Intra16x16ACLevel[ i8x8 * 4 + i4x4 ][ i ] = 0;
                    }
                }
                else
                {
                    for( i = 0; i < 16; i++ )
                    {
                        LumaLevel[ i8x8 * 4 + i4x4 ][ i ] = 0;
                    }
                }
                if( !entropy_coding_mode_flag && transform_size_8x8_flag )
                {
                    for( i = 0; i < 16; i++ )
                    {
                        LumaLevel8x8[ i8x8 ][ 4 * i + i4x4 ] = LumaLevel[ i8x8 * 4 + i4x4 ][ i ];
                    }
                }
            }
        }
        else if( CodedBlockPatternLuma & ( 1 << i8x8 ) )
        {
            residual_block( LumaLevel8x8[ i8x8 ], 64 );
        }
        else
        {
            for( i = 0; i < 64; i++ )
            {
                LumaLevel8x8[ i8x8 ][ i ] = 0;
            }
        }
    }
    if( chroma_format_idc != 0 )
    {
        NumC8x8 = 4 / ( SubWidthC * SubHeightC );
        for( iCbCr = 0; iCbCr < 2; iCbCr++ )
        {
            if( CodedBlockPatternChroma & 3 ) // chroma DC residual present
            {
                residual_block( ChromaDCLevel[ iCbCr ], 4 * NumC8x8 );
            }
            else
            {
                for( i = 0; i < 4 * NumC8x8; i++ )
                {
                    ChromaDCLevel[ iCbCr ][ i ] = 0;
                }
            }
        }
        for( iCbCr = 0; iCbCr < 2; iCbCr++ )
        {
            for( i8x8 = 0; i8x8 < NumC8x8; i8x8++ )
            {
                for( i4x4 = 0; i4x4 < 4; i4x4++ )
                {
                    if( CodedBlockPatternChroma & 2 )  // chroma AC residual present
                    {
                        residual_block( ChromaACLevel[ iCbCr ][ i8x8*4+i4x4 ], 15);
                    }
                    else
                    {
                        for( i = 0; i < 15; i++ )
                        {
                            ChromaACLevel[ iCbCr ][ i8x8*4+i4x4 ][ i ] = 0;
                        }
                    }
                }
            }
        }
    }

}


//7.3.5.3.1 Residual block CAVLC syntax
write_residual_block_cavlc( coeffLevel, maxNumCoeff )
{
    for( i = 0; i < maxNumCoeff; i++ )
    {
        coeffLevel[ i ] = 0;
    }
    bs_write_ce(b, coeff_token);
    if( TotalCoeff( coeff_token ) > 0 )
    {
        if( TotalCoeff( coeff_token ) > 10 && TrailingOnes( coeff_token ) < 3 )
        {
            suffixLength = 1;
        }
        else
        {
            suffixLength = 0;
        }
        for( i = 0; i < TotalCoeff( coeff_token ); i++ )
        {
            if( i < TrailingOnes( coeff_token ) )
            {
                bs_write_u(b, 1, trailing_ones_sign_flag);
                level[ i ] = 1 - 2 * trailing_ones_sign_flag;
            }
            else
            {
                bs_write_ce(b, level_prefix);
                levelCode = ( Min( 15, level_prefix ) << suffixLength );
                if( suffixLength > 0 || level_prefix >= 14 )
                {
                    bs_write_u(b, level_suffix);
                    levelCode += level_suffix;
                }
                if( level_prefix > = 15 && suffixLength == 0 )
                {
                    levelCode += 15;
                }
                if( level_prefix > = 16 )
                {
                    levelCode += ( 1 << ( level_prefix - 3 ) ) - 4096;
                }
                if( i == TrailingOnes( coeff_token ) &&
                    TrailingOnes( coeff_token ) < 3 )
                {
                    levelCode += 2;
                }
                if( levelCode % 2 == 0 )
                {
                    level[ i ] = ( levelCode + 2 ) >> 1;
                }
                else
                {
                    level[ i ] = ( -levelCode - 1 ) >> 1;
                }
                if( suffixLength == 0 )
                {
                    suffixLength = 1;
                }
                if( Abs( level[ i ] ) > ( 3 << ( suffixLength - 1 ) ) &&
                    suffixLength < 6 )
                {
                    suffixLength++;
                }
            }
        }
        if( TotalCoeff( coeff_token ) < maxNumCoeff )
        {
            bs_write_ce(b, total_zeros);
            zerosLeft = total_zeros;
        } else
        {
            zerosLeft = 0;
        }
        for( i = 0; i < TotalCoeff( coeff_token ) - 1; i++ )
        {
            if( zerosLeft > 0 )
            {
                bs_write_ce(b, run_before);
                run[ i ] = run_before;
            } else
            {
                run[ i ] = 0;
            }
            zerosLeft = zerosLeft - run[ i ];
        }
        run[ TotalCoeff( coeff_token ) - 1 ] = zerosLeft;
        coeffNum = -1;

        for( i = TotalCoeff( coeff_token ) - 1; i >= 0; i-- )
        {
            coeffNum += run[ i ] + 1;
            coeffLevel[ coeffNum ] = level[ i ];
        }
    }
}



//7.3.5.3.2 Residual block CABAC syntax
write_residual_block_cabac( coeffLevel, maxNumCoeff )
{
    if( maxNumCoeff == 64 )
    {
        coded_block_flag = 1;
    }
    else
    {
        bs_write_ae(b, coded_block_flag);
    }
    if( coded_block_flag )
    {
        numCoeff = maxNumCoeff;
        i=0;
        do
        {
            bs_write_ae(b, significant_coeff_flag[ i ]);
            if( significant_coeff_flag[ i ] )
            {
                bs_write_ae(b, last_significant_coeff_flag[ i ]);
                if( last_significant_coeff_flag[ i ] )
                {
                    numCoeff = i + 1;
                    for( j = numCoeff; j < maxNumCoeff; j++ )
                    {
                        coeffLevel[ j ] = 0;
                    }
                }
            }
            i++;
        } while( i < numCoeff - 1 );

        bs_write_ae(b, coeff_abs_level_minus1[ numCoeff - 1 ]);
        bs_write_ae(b, coeff_sign_flag[ numCoeff - 1 ]);
        coeffLevel[ numCoeff - 1 ] =
            ( coeff_abs_level_minus1[ numCoeff - 1 ] + 1 ) *
            ( 1 - 2 * coeff_sign_flag[ numCoeff - 1 ] );
        for( i = numCoeff - 2; i >= 0; i-- )
        {
            if( significant_coeff_flag[ i ] )
            {
                bs_write_ae(b, coeff_abs_level_minus1[ i ]);
                bs_write_ae(b, coeff_sign_flag[ i ]);
                coeffLevel[ i ] = ( coeff_abs_level_minus1[ i ] + 1 ) *
                    ( 1 - 2 * coeff_sign_flag[ i ] );
            }
            else
            {
                coeffLevel[ i ] = 0;
            }
        }
    }
    else
    {
        for( i = 0; i < maxNumCoeff; i++ )
        {
            coeffLevel[ i ] = 0;
        }
    }
}
