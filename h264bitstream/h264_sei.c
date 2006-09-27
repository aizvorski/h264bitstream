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


// D.1 SEI payload syntax

_rw(sei_payload)( h264_t* h, bs_t* b, int payloadType, int payloadSize)
{ 
    if( payloadType == 0 )
        _rw(buffering_period)( h, b, payloadSize );                   
    else if( payloadType == 1 )
        _rw(pic_timing)( h, b, payloadSize );                         
    else if( payloadType == 2 )
        _rw(pan_scan_rect)( h, b, payloadSize );                      
    else if( payloadType == 3 )
        _rw(filler_payload)( h, b, payloadSize );                    
    else if( payloadType == 4 )
        _rw(user_data_registered_itu_t_t35)( h, b, payloadSize );    
    else if( payloadType == 5 )
        _rw(user_data_unregistered)( h, b, payloadSize );            
    else if( payloadType == 6 )
        _rw(recovery_point)( h, b, payloadSize );                    
    else if( payloadType == 7 )
        _rw(dec_ref_pic_marking_repetition)( h, b, payloadSize );    
    else if( payloadType == 8 )
        _rw(spare_pic)( h, b, payloadSize );                         
    else if( payloadType == 9 )
        _rw(scene_info)( h, b, payloadSize );                        
    else if( payloadType == 10 )
        _rw(sub_seq_info)( h, b, payloadSize );                      
    else if( payloadType == 11 )
        _rw(sub_seq_layer_characteristics)( h, b, payloadSize );     
    else if( payloadType == 12 )
        _rw(sub_seq_characteristics)( h, b, payloadSize );           
    else if( payloadType == 13 )
        _rw(full_frame_freeze)( h, b, payloadSize );                 
    else if( payloadType == 14 )
        _rw(full_frame_freeze_release)( h, b, payloadSize );         
    else if( payloadType == 15 )
        _rw(full_frame_snapshot)( h, b, payloadSize );               
    else if( payloadType == 16 )
        _rw(progressive_refinement_segment_start)( h, b, payloadSize );
    else if( payloadType == 17 )
        _rw(progressive_refinement_segment_end)( h, b, payloadSize );
    else if( payloadType == 18 )
        _rw(motion_constrained_slice_group_set)( h, b, payloadSize );
    else if( payloadType == 19 )
        _rw(film_grain_characteristics)( h, b, payloadSize );         
    else if( payloadType == 20 )
        _rw(deblocking_filter_display_preference)( h, b, payloadSize ); 
    else if( payloadType == 21 )
        _rw(stereo_video_info)( h, b, payloadSize );                  
    else
        reserved_sei_message)( h, b, payloadSize );               
    if( !byte_aligned( ) ) {
        bs_f(b, 1, bit_equal_to_one /* equal to 1 */ );
        while( !byte_aligned( ) )
            bs_f(b, 1, bit_equal_to_zero /* equal to 0 */ );
    }
}


// D.1.1 Buffering period SEI message syntax
_rw(buffering_period)(h264_t* h, bs_t* b, int payloadSize) {                                           
                                                                           
        bs_ue(b, s->seq_parameter_set_id);
        if( NalHrdBpPresentFlag ) {
            for( SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {
               bs_u(b, v, s->initial_cpb_removal_delay[ SchedSelIdx ]);
               bs_u(b, v, s->initial_cpb_removal_delay_offset[ SchedSelIdx ]);
            }
        }
        if( VclHrdBpPresentFlag ) {
            for( SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {
               bs_u(b, v, s->initial_cpb_removal_delay[ SchedSelIdx ]);
               bs_u(b, v, s->initial_cpb_removal_delay_offset[ SchedSelIdx ]);
            }
        }
      }

// D.1.2 Picture timing SEI message syntax
_rw(pic_timing)(h264_t* h, bs_t* b, int payloadSize) {                
   if( CpbDpbDelaysPresentFlag ) {
                                  
       bs_u(b, v, s->cpb_removal_delay);
                                    
       bs_u(b, v, s->dpb_output_delay);
   }
   if( pic_struct_present_flag ) {
                                 
       bs_u(b, 4, s->pic_struct);
       for( i = 0; i < NumClockTS ; i++ ) {
                              
          bs_u(b, 1, s->clock_timestamp_flag[ i ]);
          if( clock_timestamp_flag[i] ) {
                                      
              bs_u(b, 2, s->ct_type);
                             
              bs_u(b, 1, s->nuit_field_based_flag);
                                      
              bs_u(b, 5, s->counting_type);
                                   
              bs_u(b, 1, s->full_timestamp_flag);
                                     
              bs_u(b, 1, s->discontinuity_flag);
                                      
              bs_u(b, 1, s->cnt_dropped_flag);
                                     
              bs_u(b, 8, s->n_frames);
              if( full_timestamp_flag ) {
                  bs_u(b, 6, s->seconds_value /* 0..59 */);
                       bs_u(b, 6, s->minutes_value /* 0..59 */);
                       bs_u(b, 5, s->hours_value /* 0..23 */);
                   } else {
                                                  
                       bs_u(b, 1, s->seconds_flag);
                       if( seconds_flag ) {
                           bs_u(b, 6, s->seconds_value /* range 0..59 */);
                                                   
                           bs_u(b, 1, s->minutes_flag);
                           if( minutes_flag ) {
                               bs_u(b, 6, s->minutes_value /* 0..59 */);
                                                   
                               bs_u(b, 1, s->hours_flag);
                               if( hours_flag )
                                   bs_u(b, 5, s->hours_value /* 0..23 */);
                           }
                       }
                   }
                   if( time_offset_length > 0 )
                                                  
                       time_offset       i(v)
               }
            }
        }
      }

// D.1.3 Pan-scan rectangle SEI message syntax
_rw(pan_scan_rect)(h264_t* h, bs_t* b, int payloadSize) {                      
                                              
        bs_ue(b, s->pan_scan_rect_id);
                                               
        bs_u(b, 1, s->pan_scan_rect_cancel_flag);
        if( !pan_scan_rect_cancel_flag ) {
                                                
            bs_ue(b, s->pan_scan_cnt_minus1);
            for( i = 0; i <= pan_scan_cnt_minus1; i++ ) {
               bs_se(b, s->pan_scan_rect_left_offset[ i ]);
               bs_se(b, s->pan_scan_rect_right_offset[ i ]);
               bs_se(b, s->pan_scan_rect_top_offset[ i ]);
               bs_se(b, s->pan_scan_rect_bottom_offset[ i ]);
            }
                                                 
            bs_ue(b, s->pan_scan_rect_repetition_period);
        }
      }

// D.1.4  Filler payload SEI message syntax
      _rw(filler_payload)(h264_t* h, bs_t* b, int payloadSize) {      
          for( k = 0; k < payloadSize; k++)
             ff_byte /* equal to 0xFF */  f(8)
      }

// D.1.5 User data registered by ITU-T Recommendation T.35 SEI message syntax
      _rw(user_data_registered_itu_t_t35)(h264_t* h, bs_t* b, int payloadSize) {
                                                            
          bs_u(b, 8, s->itu_t_t35_country_code);
          if( itu_t_t35_country_code != 0xFF )
              i=1;
          else {
                                                                
              bs_u(b, 8, s->itu_t_t35_country_code_extension_byte);
              i=2;
          }
          do {
                                                                   
              bs_u(b, 8, s->itu_t_t35_payload_byte);
              i++;
          } while( i < payloadSize )
      }

// D.1.6  User data unregistered SEI message syntax
      user_data_unregistered(h264_t* h, bs_t* b, int payloadSize) {        
                                                                     
          bs_u(b, 128, s->uuid_iso_iec_11578);
          for( i = 16; i < payloadSize; i++ )
                                                                  
              bs_u(b, 8, s->user_data_payload_byte);
      }

// D.1.7  Recovery point SEI message syntax
        recovery_point(h264_t* h, bs_t* b, int payloadSize) {                              
                                                                   
            bs_ue(b, s->recovery_frame_cnt);
                                                                   
            bs_u(b, 1, s->exact_match_flag);
                                                                   
            bs_u(b, 1, s->broken_link_flag);
                                                                   
            bs_u(b, 2, s->changing_slice_group_idc);
        }

// D.1.8  Decoded reference picture marking repetition SEI message syntax
        dec_ref_pic_marking_repetition(h264_t* h, bs_t* b, int payloadSize) {             
                                                                 
            bs_u(b, 1, s->original_idr_flag);
                                                                  
            bs_ue(b, s->original_frame_num);
            if( !frame_mbs_only_flag ) {
                                                                 
                bs_u(b, 1, s->original_field_pic_flag);
                if( original_field_pic_flag )
                                                                  
                    bs_u(b, 1, s->original_bottom_field_flag);
            }
            dec_ref_pic_marking( )                                  
        }

// D.1.9  Spare picture SEI message syntax
        spare_pic(h264_t* h, bs_t* b, int payloadSize) {                                  
                                                             
           bs_ue(b, s->target_frame_num);
                                                               
           bs_u(b, 1, s->spare_field_flag);
           if( spare_field_flag )
                                                                
               bs_u(b, 1, s->target_bottom_field_flag);
                                                                
           bs_ue(b, s->num_spare_pics_minus1);
           for( i = 0; i < num_spare_pics_minus1 + 1; i++ ) {
               bs_ue(b, s->delta_spare_frame_num[ i ]);
               if( spare_field_flag )
                   bs_u(b, 1, s->spare_bottom_field_flag[ i ]);
               bs_ue(b, s->spare_area_idc[ i ]);
               if( spare_area_idc[ i ] == 1 )
                   for( j = 0; j < PicSizeInMapUnits; j++ )
                      bs_u(b, 1, s->spare_unit_flag[ i ][ j ]);
               else if( spare_area_idc[ i ] == 2 ) {
                   mapUnitCnt = 0
                   for( j=0; mapUnitCnt < PicSizeInMapUnits; j++ ) {
                      bs_ue(b, s->zero_run_length[ i ][ j ]);
                      mapUnitCnt += zero_run_length[ i ][ j ] + 1
                   }
               }
           }
        }

// D.1.10 Scene information SEI message syntax
        scene_info(h264_t* h, bs_t* b, int payloadSize) {                               
                                                                
           bs_u(b, 1, s->scene_info_present_flag);
           if( scene_info_present_flag ) {
                                                               
               bs_ue(b, s->scene_id);
                                                              
               bs_ue(b, s->scene_transition_type);
               if( scene_transition_type > 3 )
                                                            
                   bs_ue(b, s->second_scene_id);
           }
        }

// D.1.11 Sub-sequence information SEI message syntax
        sub_seq_info(h264_t* h, bs_t* b, int payloadSize) {                                   
                                                                   
          bs_ue(b, s->sub_seq_layer_num);
                                                                     
          bs_ue(b, s->sub_seq_id);
                                                                    
          bs_u(b, 1, s->first_ref_pic_flag);
                                                                   
          bs_u(b, 1, s->leading_non_ref_pic_flag);
                                                                    
          bs_u(b, 1, s->last_pic_flag);
                                                                    
          bs_u(b, 1, s->sub_seq_frame_num_flag);
          if( sub_seq_frame_num_flag )
                                                                      
              bs_ue(b, s->sub_seq_frame_num);
        }

// D.1.12 Sub-sequence layer characteristics SEI message syntax
        sub_seq_layer_characteristics(h264_t* h, bs_t* b, int payloadSize) {                    
                                                               
          bs_ue(b, s->num_sub_seq_layers_minus1);
          for( layer = 0; layer <= num_sub_seq_layers_minus1; layer++ ) {
                                                                    
              bs_u(b, 1, s->accurate_statistics_flag);
                                                                      
              bs_u(b, 16, s->average_bit_rate);
                                                                     
              bs_u(b, 16, s->average_frame_rate);
          }
        }

// D.1.13 Sub-sequence characteristics SEI message syntax
        sub_seq_characteristics(h264_t* h, bs_t* b, int payloadSize) {                          
                                                            
          bs_ue(b, s->sub_seq_layer_num);
                                                                  
          bs_ue(b, s->sub_seq_id);
                                                                    
          bs_u(b, 1, s->duration_flag);
          if( duration_flag)
                                                                  
              bs_u(b, 32, s->sub_seq_duration);
                                                                  
          bs_u(b, 1, s->average_rate_flag);
          if( average_rate_flag ) {
                                                                   
              bs_u(b, 1, s->accurate_statistics_flag);
                                                                     
              bs_u(b, 16, s->average_bit_rate);
                                                                      
              bs_u(b, 16, s->average_frame_rate);
          }
                                                                
          bs_ue(b, s->num_referenced_subseqs);
          for( n = 0; n < num_referenced_subseqs; n++ ) {
                                                                  
              bs_ue(b, s->ref_sub_seq_layer_num);
                                                                    
              bs_ue(b, s->ref_sub_seq_id);
                                                                  
              bs_u(b, 1, s->ref_sub_seq_direction);
          }
        }

// D.1.14 Full-frame freeze SEI message syntax
       full_frame_freeze(h264_t* h, bs_t* b, int payloadSize) {                      
                                                          
                                                             
          bs_ue(b, s->full_frame_freeze_repetition_period);
       }

// D.1.15 Full-frame freeze release SEI message syntax
       full_frame_freeze_release(h264_t* h, bs_t* b, int payloadSize) {              
       }

// D.1.16 Full-frame snapshot SEI message syntax
       full_frame_snapshot(h264_t* h, bs_t* b, int payloadSize) {                    
                                                      
          bs_ue(b, s->snapshot_id);
       }

// D.1.17 Progressive refinement segment start SEI message syntax
       progressive_refinement_segment_start(h264_t* h, bs_t* b, int payloadSize) {   
                                                       
          bs_ue(b, s->progressive_refinement_id);
                                                      
          bs_ue(b, s->num_refinement_steps_minus1);
       }

// D.1.18 Progressive refinement segment end SEI message syntax
       progressive_refinement_segment_end(h264_t* h, bs_t* b, int payloadSize) {     
                                                         
          bs_ue(b, s->progressive_refinement_id);
       }

// D.1.19 Motion-constrained slice group set SEI message syntax
       motion_constrained_slice_group_set(h264_t* h, bs_t* b, int payloadSize) {     
                                                        
          bs_ue(b, s->num_slice_groups_in_set_minus1);
          for( i = 0; i <= num_slice_groups_in_set_minus1; i++)
              bs_u(b, v, s->slice_group_id[ i ]);
                                                       
          bs_u(b, 1, s->exact_sample_value_match_flag);
                                                       
          bs_u(b, 1, s->pan_scan_rect_flag);
          if( pan_scan_rect_flag )
                                                    
              bs_ue(b, s->pan_scan_rect_id);
       }

// D.1.20 Film grain characteristics SEI message syntax
       film_grain_characteristics(h264_t* h, bs_t* b, int payloadSize) {                                 
                                                                       
          bs_u(b, 1, s->film_grain_characteristics_cancel_flag);
          if( !film_grain_characteristics_cancel_flag ) {
                                                                       
              bs_u(b, 2, s->model_id);
                                                                          
              bs_u(b, 1, s->separate_colour_description_present_flag);
              if( separate_colour_description_present_flag ) {
                                                                          
                  bs_u(b, 3, s->film_grain_bit_depth_luma_minus8);
                                                                            
                  bs_u(b, 3, s->film_grain_bit_depth_chroma_minus8);
                                                                           
                  bs_u(b, 1, s->film_grain_full_range_flag);
                                                                             
                  bs_u(b, 8, s->film_grain_colour_primaries);
                                                                            
                  bs_u(b, 8, s->film_grain_transfer_characteristics);
                                                                           
                  bs_u(b, 8, s->film_grain_matrix_coefficients);
              }
                                                                          
              bs_u(b, 2, s->blending_mode_id);
                                                                          
              bs_u(b, 4, s->log2_scale_factor);
              for( c = 0; c < 3; c++ )
                  bs_u(b, 1, s->comp_model_present_flag[ c ]);
              for( c = 0; c < 3; c++ )
                  if( comp_model_present_flag[ c ] ) {
                      bs_u(b, 8, s->num_intensity_intervals_minus1[ c ]);
                      bs_u(b, 3, s->num_model_values_minus1[ c ]);
                      for( i = 0; i <= num_intensity_intervals_minus1[ c ]; i++ ) {
                         bs_u(b, 8, s->intensity_interval_lower_bound[ c ][ i ]);
                         bs_u(b, 8, s->intensity_interval_upper_bound[ c ][ i ]);
                         for( j = 0; j <= num_model_values_minus1[ c ]; j++ )
                             bs_se(b, s->comp_model_value[ c ][ i ][ j ]);
                      }
                  }
                                                                        
              bs_ue(b, s->film_grain_characteristics_repetition_period);
          }
       }

// D.1.21 Deblocking filter display preference SEI message syntax
       deblocking_filter_display_preference(h264_t* h, bs_t* b, int payloadSize) {                       
                                                                         
          bs_u(b, 1, s->deblocking_display_preference_cancel_flag);
          if( !s->deblocking_display_preference_cancel_flag ) {
                                                                         
              bs_u(b, 1, s->display_prior_to_deblocking_preferred_flag);
                                                                           
              bs_u(b, 1, s->dec_frame_buffering_constraint_flag);
                                                                            
              bs_ue(b, s->deblocking_display_preference_repetition_period);
          }
       }

// D.1.22 Stereo video information SEI message syntax
         stereo_video_info(h264_t* h, bs_t* b, int payloadSize) {       
                                          
             bs_u(b, 1, s->field_views_flag);
             if( field_views_flag ) 
                                          
                 bs_u(b, 1, s->top_field_is_left_view_flag);
             else {
                                            
                 bs_u(b, 1, s->current_frame_is_left_view_flag);
                                           
                 bs_u(b, 1, s->next_frame_is_second_view_flag);
             }
                                           
             bs_u(b, 1, s->left_view_self_contained_flag);
                                           
             bs_u(b, 1, s->right_view_self_contained_flag);
         }

// D.1.23 Reserved SEI message syntax
       reserved_sei_message(h264_t* h, bs_t* b, int payloadSize) {      
           for( i = 0; i < payloadSize; i++ )
                                           
               bs_u(b, 8, s->reserved_sei_message_payload_byte);
       }
