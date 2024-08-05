/*
 * Rafael Micro R820T driver for AIRSPY
 *
 * Copyright 2013 Youssef Touil <youssef@airspy.com>
 * Copyright 2014-2016 Benjamin Vernoux <bvernoux@airspy.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "r820t.h"
#include <thread>

static int r820t_read_cache_reg(r820t_priv_t *priv, int reg);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Tuner frequency ranges */
struct r820t_freq_range
{
  uint8_t open_d;
  uint8_t rf_mux_ploy;
  uint8_t tf_c;
};

#define R820T_READ_MAX_DATA 32
#define R820T_INIT_NB_REGS (32-5)
uint8_t r820t_read_data[R820T_READ_MAX_DATA]; /* Buffer for data read from I2C */
uint8_t r820t_state_standby = 1; /* 1=standby/power off 0=r820t initialized/power on */

/* Tuner frequency ranges
"Copyright (C) 2013 Mauro Carvalho Chehab"
https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
part of freq_ranges()
*/
const struct r820t_freq_range freq_ranges[] =
{
  {
  /* 0 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xdf, /* R27[7:0]  band2,band0 */
  }, {
  /* 50 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0xbe, /* R27[7:0]  band4,band1  */
  }, {
  /* 55 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x8b, /* R27[7:0]  band7,band4 */
  }, {
  /* 60 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x7b, /* R27[7:0]  band8,band4 */
  }, {
  /* 65 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x69, /* R27[7:0]  band9,band6 */
  }, {
  /* 70 MHz */
  /* .open_d = */     0x08, /* low */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x58, /* R27[7:0]  band10,band7 */
  }, {
  /* 75 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 80 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x44, /* R27[7:0]  band11,band11 */
  }, {
  /* 90 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 100 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x34, /* R27[7:0]  band12,band11 */
  }, {
  /* 110 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 120 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x24, /* R27[7:0]  band13,band11 */
  }, {
  /* 140 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x14, /* R27[7:0]  band14,band11 */
  }, {
  /* 180 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 220 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x13, /* R27[7:0]  band14,band12 */
  }, {
  /* 250 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x11, /* R27[7:0]  highest,highest */
  }, {
  /* 280 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x02, /* R26[7:6]=0 (LPF)  R26[1:0]=2 (low) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 310 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 450 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x41, /* R26[7:6]=1 (bypass)  R26[1:0]=1 (middle) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 588 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }, {
  /* 650 MHz */
  /* .open_d = */     0x00, /* high */
  /* .rf_mux_ploy = */  0x40, /* R26[7:6]=1 (bypass)  R26[1:0]=0 (highest) */
  /* .tf_c = */     0x00, /* R27[7:0]  highest,highest */
  }
};

#define FREQ_TO_IDX_SIZE (600)
const uint8_t freq_to_idx[FREQ_TO_IDX_SIZE]=
{
  /* 50 */ 1,/* 51 */ 1,/* 52 */ 1,/* 53 */ 1,/* 54 */ 1,
  /* 55 */ 2,/* 56 */ 2,/* 57 */ 2,/* 58 */ 2,/* 59 */ 2,
  /* 60 */ 3,/* 61 */ 3,/* 62 */ 3,/* 63 */ 3,/* 64 */ 3,
  /* 65 */ 4,/* 66 */ 4,/* 67 */ 4,/* 68 */ 4,/* 69 */ 4,
  /* 70 */ 5,/* 71 */ 5,/* 72 */ 5,/* 73 */ 5,/* 74 */ 5,
  /* 75 */ 6,/* 76 */ 6,/* 77 */ 6,/* 78 */ 6,/* 79 */ 6,
  /* 80 */ 7,/* 81 */ 7,/* 82 */ 7,/* 83 */ 7,/* 84 */ 7,/* 85 */ 7,/* 86 */ 7,/* 87 */ 7,/* 88 */ 7,/* 89 */ 7,
  /* 90 */ 8,/* 91 */ 8,/* 92 */ 8,/* 93 */ 8,/* 94 */ 8,/* 95 */ 8,/* 96 */ 8,/* 97 */ 8,/* 98 */ 8,/* 99 */ 8,
  /* 100 */ 9,/* 101 */ 9,/* 102 */ 9,/* 103 */ 9,/* 104 */ 9,/* 105 */ 9,/* 106 */ 9,/* 107 */ 9,/* 108 */ 9,/* 109 */ 9,
  /* 110 */ 10,/* 111 */ 10,/* 112 */ 10,/* 113 */ 10,/* 114 */ 10,/* 115 */ 10,/* 116 */ 10,/* 117 */ 10,/* 118 */ 10,/* 119 */ 10,
  /* 120 */ 11,/* 121 */ 11,/* 122 */ 11,/* 123 */ 11,/* 124 */ 11,/* 125 */ 11,/* 126 */ 11,/* 127 */ 11,/* 128 */ 11,/* 129 */ 11,
  /* 130 */ 11,/* 131 */ 11,/* 132 */ 11,/* 133 */ 11,/* 134 */ 11,/* 135 */ 11,/* 136 */ 11,/* 137 */ 11,/* 138 */ 11,/* 139 */ 11,
  /* 140 */ 12,/* 141 */ 12,/* 142 */ 12,/* 143 */ 12,/* 144 */ 12,/* 145 */ 12,/* 146 */ 12,/* 147 */ 12,/* 148 */ 12,/* 149 */ 12,
  /* 150 */ 12,/* 151 */ 12,/* 152 */ 12,/* 153 */ 12,/* 154 */ 12,/* 155 */ 12,/* 156 */ 12,/* 157 */ 12,/* 158 */ 12,/* 159 */ 12,
  /* 160 */ 12,/* 161 */ 12,/* 162 */ 12,/* 163 */ 12,/* 164 */ 12,/* 165 */ 12,/* 166 */ 12,/* 167 */ 12,/* 168 */ 12,/* 169 */ 12,
  /* 170 */ 12,/* 171 */ 12,/* 172 */ 12,/* 173 */ 12,/* 174 */ 12,/* 175 */ 12,/* 176 */ 12,/* 177 */ 12,/* 178 */ 12,/* 179 */ 12,
  /* 180 */ 13,/* 181 */ 13,/* 182 */ 13,/* 183 */ 13,/* 184 */ 13,/* 185 */ 13,/* 186 */ 13,/* 187 */ 13,/* 188 */ 13,/* 189 */ 13,
  /* 190 */ 13,/* 191 */ 13,/* 192 */ 13,/* 193 */ 13,/* 194 */ 13,/* 195 */ 13,/* 196 */ 13,/* 197 */ 13,/* 198 */ 13,/* 199 */ 13,
  /* 200 */ 13,/* 201 */ 13,/* 202 */ 13,/* 203 */ 13,/* 204 */ 13,/* 205 */ 13,/* 206 */ 13,/* 207 */ 13,/* 208 */ 13,/* 209 */ 13,
  /* 210 */ 13,/* 211 */ 13,/* 212 */ 13,/* 213 */ 13,/* 214 */ 13,/* 215 */ 13,/* 216 */ 13,/* 217 */ 13,/* 218 */ 13,/* 219 */ 13,
  /* 220 */ 14,/* 221 */ 14,/* 222 */ 14,/* 223 */ 14,/* 224 */ 14,/* 225 */ 14,/* 226 */ 14,/* 227 */ 14,/* 228 */ 14,/* 229 */ 14,
  /* 230 */ 14,/* 231 */ 14,/* 232 */ 14,/* 233 */ 14,/* 234 */ 14,/* 235 */ 14,/* 236 */ 14,/* 237 */ 14,/* 238 */ 14,/* 239 */ 14,
  /* 240 */ 14,/* 241 */ 14,/* 242 */ 14,/* 243 */ 14,/* 244 */ 14,/* 245 */ 14,/* 246 */ 14,/* 247 */ 14,/* 248 */ 14,/* 249 */ 14,
  /* 250 */ 15,/* 251 */ 15,/* 252 */ 15,/* 253 */ 15,/* 254 */ 15,/* 255 */ 15,/* 256 */ 15,/* 257 */ 15,/* 258 */ 15,/* 259 */ 15,
  /* 260 */ 15,/* 261 */ 15,/* 262 */ 15,/* 263 */ 15,/* 264 */ 15,/* 265 */ 15,/* 266 */ 15,/* 267 */ 15,/* 268 */ 15,/* 269 */ 15,
  /* 270 */ 15,/* 271 */ 15,/* 272 */ 15,/* 273 */ 15,/* 274 */ 15,/* 275 */ 15,/* 276 */ 15,/* 277 */ 15,/* 278 */ 15,/* 279 */ 15,
  /* 280 */ 16,/* 281 */ 16,/* 282 */ 16,/* 283 */ 16,/* 284 */ 16,/* 285 */ 16,/* 286 */ 16,/* 287 */ 16,/* 288 */ 16,/* 289 */ 16,
  /* 290 */ 16,/* 291 */ 16,/* 292 */ 16,/* 293 */ 16,/* 294 */ 16,/* 295 */ 16,/* 296 */ 16,/* 297 */ 16,/* 298 */ 16,/* 299 */ 16,
  /* 300 */ 16,/* 301 */ 16,/* 302 */ 16,/* 303 */ 16,/* 304 */ 16,/* 305 */ 16,/* 306 */ 16,/* 307 */ 16,/* 308 */ 16,/* 309 */ 16,
  /* 310 */ 17,/* 311 */ 17,/* 312 */ 17,/* 313 */ 17,/* 314 */ 17,/* 315 */ 17,/* 316 */ 17,/* 317 */ 17,/* 318 */ 17,/* 319 */ 17,
  /* 320 */ 17,/* 321 */ 17,/* 322 */ 17,/* 323 */ 17,/* 324 */ 17,/* 325 */ 17,/* 326 */ 17,/* 327 */ 17,/* 328 */ 17,/* 329 */ 17,
  /* 330 */ 17,/* 331 */ 17,/* 332 */ 17,/* 333 */ 17,/* 334 */ 17,/* 335 */ 17,/* 336 */ 17,/* 337 */ 17,/* 338 */ 17,/* 339 */ 17,
  /* 340 */ 17,/* 341 */ 17,/* 342 */ 17,/* 343 */ 17,/* 344 */ 17,/* 345 */ 17,/* 346 */ 17,/* 347 */ 17,/* 348 */ 17,/* 349 */ 17,
  /* 350 */ 17,/* 351 */ 17,/* 352 */ 17,/* 353 */ 17,/* 354 */ 17,/* 355 */ 17,/* 356 */ 17,/* 357 */ 17,/* 358 */ 17,/* 359 */ 17,
  /* 360 */ 17,/* 361 */ 17,/* 362 */ 17,/* 363 */ 17,/* 364 */ 17,/* 365 */ 17,/* 366 */ 17,/* 367 */ 17,/* 368 */ 17,/* 369 */ 17,
  /* 370 */ 17,/* 371 */ 17,/* 372 */ 17,/* 373 */ 17,/* 374 */ 17,/* 375 */ 17,/* 376 */ 17,/* 377 */ 17,/* 378 */ 17,/* 379 */ 17,
  /* 380 */ 17,/* 381 */ 17,/* 382 */ 17,/* 383 */ 17,/* 384 */ 17,/* 385 */ 17,/* 386 */ 17,/* 387 */ 17,/* 388 */ 17,/* 389 */ 17,
  /* 390 */ 17,/* 391 */ 17,/* 392 */ 17,/* 393 */ 17,/* 394 */ 17,/* 395 */ 17,/* 396 */ 17,/* 397 */ 17,/* 398 */ 17,/* 399 */ 17,
  /* 400 */ 17,/* 401 */ 17,/* 402 */ 17,/* 403 */ 17,/* 404 */ 17,/* 405 */ 17,/* 406 */ 17,/* 407 */ 17,/* 408 */ 17,/* 409 */ 17,
  /* 410 */ 17,/* 411 */ 17,/* 412 */ 17,/* 413 */ 17,/* 414 */ 17,/* 415 */ 17,/* 416 */ 17,/* 417 */ 17,/* 418 */ 17,/* 419 */ 17,
  /* 420 */ 17,/* 421 */ 17,/* 422 */ 17,/* 423 */ 17,/* 424 */ 17,/* 425 */ 17,/* 426 */ 17,/* 427 */ 17,/* 428 */ 17,/* 429 */ 17,
  /* 430 */ 17,/* 431 */ 17,/* 432 */ 17,/* 433 */ 17,/* 434 */ 17,/* 435 */ 17,/* 436 */ 17,/* 437 */ 17,/* 438 */ 17,/* 439 */ 17,
  /* 440 */ 17,/* 441 */ 17,/* 442 */ 17,/* 443 */ 17,/* 444 */ 17,/* 445 */ 17,/* 446 */ 17,/* 447 */ 17,/* 448 */ 17,/* 449 */ 17,
  /* 450 */ 18,/* 451 */ 18,/* 452 */ 18,/* 453 */ 18,/* 454 */ 18,/* 455 */ 18,/* 456 */ 18,/* 457 */ 18,/* 458 */ 18,/* 459 */ 18,
  /* 460 */ 18,/* 461 */ 18,/* 462 */ 18,/* 463 */ 18,/* 464 */ 18,/* 465 */ 18,/* 466 */ 18,/* 467 */ 18,/* 468 */ 18,/* 469 */ 18,
  /* 470 */ 18,/* 471 */ 18,/* 472 */ 18,/* 473 */ 18,/* 474 */ 18,/* 475 */ 18,/* 476 */ 18,/* 477 */ 18,/* 478 */ 18,/* 479 */ 18,
  /* 480 */ 18,/* 481 */ 18,/* 482 */ 18,/* 483 */ 18,/* 484 */ 18,/* 485 */ 18,/* 486 */ 18,/* 487 */ 18,/* 488 */ 18,/* 489 */ 18,
  /* 490 */ 18,/* 491 */ 18,/* 492 */ 18,/* 493 */ 18,/* 494 */ 18,/* 495 */ 18,/* 496 */ 18,/* 497 */ 18,/* 498 */ 18,/* 499 */ 18,
  /* 500 */ 18,/* 501 */ 18,/* 502 */ 18,/* 503 */ 18,/* 504 */ 18,/* 505 */ 18,/* 506 */ 18,/* 507 */ 18,/* 508 */ 18,/* 509 */ 18,
  /* 510 */ 18,/* 511 */ 18,/* 512 */ 18,/* 513 */ 18,/* 514 */ 18,/* 515 */ 18,/* 516 */ 18,/* 517 */ 18,/* 518 */ 18,/* 519 */ 18,
  /* 520 */ 18,/* 521 */ 18,/* 522 */ 18,/* 523 */ 18,/* 524 */ 18,/* 525 */ 18,/* 526 */ 18,/* 527 */ 18,/* 528 */ 18,/* 529 */ 18,
  /* 530 */ 18,/* 531 */ 18,/* 532 */ 18,/* 533 */ 18,/* 534 */ 18,/* 535 */ 18,/* 536 */ 18,/* 537 */ 18,/* 538 */ 18,/* 539 */ 18,
  /* 540 */ 18,/* 541 */ 18,/* 542 */ 18,/* 543 */ 18,/* 544 */ 18,/* 545 */ 18,/* 546 */ 18,/* 547 */ 18,/* 548 */ 18,/* 549 */ 18,
  /* 550 */ 18,/* 551 */ 18,/* 552 */ 18,/* 553 */ 18,/* 554 */ 18,/* 555 */ 18,/* 556 */ 18,/* 557 */ 18,/* 558 */ 18,/* 559 */ 18,
  /* 560 */ 18,/* 561 */ 18,/* 562 */ 18,/* 563 */ 18,/* 564 */ 18,/* 565 */ 18,/* 566 */ 18,/* 567 */ 18,/* 568 */ 18,/* 569 */ 18,
  /* 570 */ 18,/* 571 */ 18,/* 572 */ 18,/* 573 */ 18,/* 574 */ 18,/* 575 */ 18,/* 576 */ 18,/* 577 */ 18,/* 578 */ 18,/* 579 */ 18,
  /* 580 */ 18,/* 581 */ 18,/* 582 */ 18,/* 583 */ 18,/* 584 */ 18,/* 585 */ 18,/* 586 */ 18,/* 587 */ 18,
  /* 588 */ 19,/* 589 */ 19,/* 590 */ 19,/* 591 */ 19,/* 592 */ 19,/* 593 */ 19,/* 594 */ 19,/* 595 */ 19,/* 596 */ 19,/* 597 */ 19,
  /* 598 */ 19,/* 599 */ 19,/* 600 */ 19,/* 601 */ 19,/* 602 */ 19,/* 603 */ 19,/* 604 */ 19,/* 605 */ 19,/* 606 */ 19,/* 607 */ 19,
  /* 608 */ 19,/* 609 */ 19,/* 610 */ 19,/* 611 */ 19,/* 612 */ 19,/* 613 */ 19,/* 614 */ 19,/* 615 */ 19,/* 616 */ 19,/* 617 */ 19,
  /* 618 */ 19,/* 619 */ 19,/* 620 */ 19,/* 621 */ 19,/* 622 */ 19,/* 623 */ 19,/* 624 */ 19,/* 625 */ 19,/* 626 */ 19,/* 627 */ 19,
  /* 628 */ 19,/* 629 */ 19,/* 630 */ 19,/* 631 */ 19,/* 632 */ 19,/* 633 */ 19,/* 634 */ 19,/* 635 */ 19,/* 636 */ 19,/* 637 */ 19,
  /* 638 */ 19,/* 639 */ 19,/* 640 */ 19,/* 641 */ 19,/* 642 */ 19,/* 643 */ 19,/* 644 */ 19,/* 645 */ 19,/* 646 */ 19,/* 647 */ 19,
  /* 648 */ 19,/* 649 */ 19
};

#define FREQ_50MHZ (50)
#define FREQ_TO_IDX_0_TO_49MHZ (0)
#define FREQ_TO_IDX_650_TO_1800MHZ (20)

int r820t_freq_get_idx(uint32_t freq_mhz)
{
  uint32_t freq_mhz_fix;

  if(freq_mhz < FREQ_50MHZ)
  {
    /* Frequency Less than 50MHz */
    return FREQ_TO_IDX_0_TO_49MHZ;
  }else
  {
    /* Frequency Between 50 to 649MHz use table */
    /* Fix the frequency for the table */
    freq_mhz_fix = freq_mhz - FREQ_50MHZ;
    if(freq_mhz_fix < FREQ_TO_IDX_SIZE)
    {

      return freq_to_idx[freq_mhz_fix];
    }else
    {
      /* Frequency Between 650 to 1800MHz */
      return FREQ_TO_IDX_650_TO_1800MHZ;
    }
  }
}

static inline bool r820t_is_power_enabled(void)
{
  return true;
}

/*
 * Write regs 5 to 32 (R820T_INIT_NB_REGS values) using data parameter and write last reg to 0
 */
void airspy_r820t_write_init(r820t_priv_t *priv, const uint8_t* data)
{
  for (int i = 0; i < R820T_INIT_NB_REGS; i++) {
    priv->write_reg(i+REG_SHADOW_START, data[i], priv->ctx);
  }
  priv->write_reg(0x1F, 0, priv->ctx);
}

/*
 * Read from one or more contiguous registers. data[0] should be the first
 * register number, one or more values follow.
 */
 const uint8_t lut[16] = { 0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf };

static uint8_t r82xx_bitrev(uint8_t byte)
{
 return (lut[byte & 0xf] << 4) | lut[byte >> 4];
}

static int r820t_write_reg(r820t_priv_t *priv, uint8_t reg, uint8_t val)
{
  if (r820t_read_cache_reg(priv, reg) == val)
    return 0;
  priv->write_reg(reg, val, priv->ctx);
  priv->regs[reg - REG_SHADOW_START] = val;
  return 0;
}

static int r820t_read_cache_reg(r820t_priv_t *priv, int reg)
{
  reg -= REG_SHADOW_START;

  if (reg >= 0 && reg < NUM_REGS)
    return priv->regs[reg];
  else
    return -1;
}

static int r820t_write_reg_mask(r820t_priv_t *priv, uint8_t reg, uint8_t val, uint8_t bit_mask)
{
  int rc = r820t_read_cache_reg(priv, reg);

  if (rc < 0)
    return rc;

  val = (rc & ~bit_mask) | (val & bit_mask);

  return r820t_write_reg(priv, reg, val);
}

static int r820t_read(r820t_priv_t *priv, uint8_t *val, int len)
{
  /* reg not used and assumed to be always 0 because start from reg0 to reg0+len */
  priv->read_reg(val, len, priv->ctx);

  return 0;
}

/*
 * r820t tuning logic
 */
#ifdef OPTIM_SET_MUX
int r820t_set_mux_freq_idx = -1; /* Default set to invalid value in order to force set_mux */
#endif

/*
"inspired by Mauro Carvalho Chehab set_mux technique"
https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
part of r820t_set_mux() (set tracking filter)
*/
static int r820t_set_tf(r820t_priv_t *priv, uint32_t freq)
{
  const struct r820t_freq_range *range;
  int freq_idx;
  int rc = 0;

  /* Get the proper frequency range in MHz instead of Hz */
  /* Fast divide freq by 1000000 */
  freq = (uint32_t)((uint64_t)freq * 4295 >> 32);

  freq_idx = r820t_freq_get_idx(freq);
  range = &freq_ranges[freq_idx];

  /* Only reconfigure mux freq if modified vs previous range */
#ifdef OPTIM_SET_MUX
  if(freq_idx != r820t_set_mux_freq_idx)
  {
#endif
    /* Open Drain */
    rc = r820t_write_reg_mask(priv, 0x17, range->open_d, 0x08);
    if (rc < 0)
      return rc;

    /* RF_MUX,Polymux */
    rc = r820t_write_reg_mask(priv, 0x1a, range->rf_mux_ploy, 0xc3);
    if (rc < 0)
      return rc;

    /* TF BAND */
    rc = r820t_write_reg(priv, 0x1b, range->tf_c);
    if (rc < 0)
      return rc;

    /* XTAL CAP & Drive */
    rc = r820t_write_reg_mask(priv, 0x10, 0x08, 0x0b);
    if (rc < 0)
      return rc;

    rc = r820t_write_reg_mask(priv, 0x08, 0x00, 0x3f);
    if (rc < 0)
      return rc;

    rc = r820t_write_reg_mask(priv, 0x09, 0x00, 0x3f);
#ifdef OPTIM_SET_MUX
  }
  r820t_set_mux_freq_idx = freq_idx;
#endif

  return rc;
}

int r820t_set_pll(r820t_priv_t *priv, uint32_t freq)
{
  const uint32_t vco_min = 1770000000;
  const uint32_t vco_max = 3900000000;
  uint32_t ref = priv->xtal_freq >> 1;

  int rc;
  uint32_t div_num;
  uint32_t vco;
  uint32_t rem;
  uint32_t mask;
  uint16_t sdm;
  uint8_t nint;
  uint8_t ni;
  uint8_t si;
  uint8_t div_found;

  /* Find a suitable divider */
  div_found = 0;
  for (div_num = 0; div_num <= 5; div_num++)
  {
    vco = freq << (div_num + 1);
    if (vco >= vco_min && vco <= vco_max)
    {
      div_found = 1;
      break;
    }
  }

  if (!div_found)
    return -1;

  vco += ref >> 16;
  ref <<= 8;
  mask = 1 << 23;
  rem = 0;
  while (mask > 0 && vco > 0)
  {
    if (vco >= ref)
    {
      rem |= mask;
      vco -= ref;
    }
    ref >>= 1;
    mask >>= 1;
  }

  nint = rem >> 16;
  sdm = rem & 0xffff;

  nint -= 13;
  ni = nint >> 2;
  si = nint & 3;

  /* Set the phase splitter */
  rc = r820t_write_reg_mask(priv, 0x10, (uint8_t) (div_num << 5), 0xe0);
  if(rc < 0)
    return rc;

  /* Set the integer part of the PLL */
  rc = r820t_write_reg(priv, 0x14, (uint8_t) (ni + (si << 6)));
  if(rc < 0)
    return rc;

  if (sdm == 0)
  {
    /* Disable SDM */
    rc = r820t_write_reg_mask(priv, 0x12, 0x08, 0x08);
    if(rc < 0)
      return rc;
  }
  else
  {
	/* Write SDM */
    rc = r820t_write_reg(priv, 0x15, (uint8_t)(sdm & 0xff));
    if (rc < 0)
      return rc;

    rc = r820t_write_reg(priv, 0x16, (uint8_t)(sdm >> 8));
    if (rc < 0)
      return rc;

    /* Enable SDM */
    rc = r820t_write_reg_mask(priv, 0x12, 0x00, 0x08);
    if (rc < 0)
      return rc;
  }
  return rc;
}

int r820t_set_freq(r820t_priv_t *priv, uint32_t freq)
{
  int rc;
  uint32_t lo_freq = freq + priv->if_freq;

  rc = r820t_set_tf(priv, freq);
  if (rc < 0)
    return rc;

  rc = r820t_set_pll(priv, lo_freq);
  if (rc < 0)
    return rc;

  priv->freq = freq;

  return 0;
}

int r820t_set_lna_gain(r820t_priv_t *priv, uint8_t gain_index)
{
  return r820t_write_reg_mask(priv, 0x05, gain_index, 0x0f);
}

int r820t_set_mixer_gain(r820t_priv_t *priv, uint8_t gain_index)
{
  return r820t_write_reg_mask(priv, 0x07, gain_index, 0x0f);
}

int r820t_set_vga_gain(r820t_priv_t *priv, uint8_t gain_index)
{
  return r820t_write_reg_mask(priv, 0x0c, gain_index, 0x0f);
}

int r820t_set_lna_agc(r820t_priv_t *priv, uint8_t value)
{
  value = value != 0 ? 0x00 : 0x10;
  return r820t_write_reg_mask(priv, 0x05, value, 0x10);
}

int r820t_set_mixer_agc(r820t_priv_t *priv, uint8_t value)
{
  value = value != 0 ? 0x10 : 0x00;
  return r820t_write_reg_mask(priv, 0x07, value, 0x10);
}

/* 
"inspired by Mauro Carvalho Chehab calibration technique"
https://stuff.mit.edu/afs/sipb/contrib/linux/drivers/media/tuners/r820t.c
part of r820t_set_tv_standard()
*/
int r820t_calibrate(r820t_priv_t *priv)
{
  int i, rc, cal_code;
  uint8_t data[5];

  for (i = 0; i < 5; i++)
  {
    /* Set filt_cap */
    rc = r820t_write_reg_mask(priv, 0x0b, 0x08, 0x60);
    if (rc < 0)
      return rc;

    /* set cali clk =on */
    rc = r820t_write_reg_mask(priv, 0x0f, 0x04, 0x04);
    if (rc < 0)
      return rc;

    /* X'tal cap 0pF for PLL */
    rc = r820t_write_reg_mask(priv, 0x10, 0x00, 0x03);
    if (rc < 0)
      return rc;

    rc = r820t_set_pll(priv, CALIBRATION_LO * 1000);
    if (rc < 0)
      return rc;

    /* Start Trigger */
    rc = r820t_write_reg_mask(priv, 0x0b, 0x10, 0x10);
    if (rc < 0)
      return rc;

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /* Stop Trigger */
    rc = r820t_write_reg_mask(priv, 0x0b, 0x00, 0x10);
    if (rc < 0)
      return rc;

    /* set cali clk =off */
    rc = r820t_write_reg_mask(priv, 0x0f, 0x00, 0x04);
    if (rc < 0)
      return rc;

    /* Check if calibration worked */
    rc = r820t_read(priv, data, sizeof(data));
    if (rc < 0)
      return rc;

    cal_code = data[4] & 0x0f;
    if (cal_code && cal_code != 0x0f)
      return 0;
  }

  return -1;
}

int r820t_init(r820t_priv_t *priv, const uint32_t if_freq, r820t_write_reg_f write_reg, r820t_read_f read_reg, void* ctx)
{
  int rc;
  uint32_t saved_freq;

  r820t_state_standby = 0;
  priv->if_freq = if_freq;
  priv->write_reg = write_reg;
  priv->read_reg = read_reg;
  priv->ctx = ctx;
  /* Initialize registers */
  airspy_r820t_write_init(priv, priv->regs);

  r820t_set_freq(priv, priv->freq);

  /* Calibrate the IF filter */
  saved_freq = priv->freq;
  rc = r820t_calibrate(priv);
  priv->freq = saved_freq;
  if (rc < 0)
  {
    saved_freq = priv->freq;
    r820t_calibrate(priv);
    priv->freq = saved_freq;
  }

  /* Restore freq as it has been modified by r820t_calibrate() */
  rc = r820t_set_freq(priv, priv->freq);
  return rc;
}

void r820t_set_if_bandwidth(r820t_priv_t *priv, uint8_t bw)
{
    const uint8_t modes[] = { 0xE0, 0x80, 0x60, 0x00 };
    const uint8_t opt[] = { 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
    uint8_t a = 0xB0 | opt[bw & 0x0F];
    uint8_t b = 0x0F | modes[bw >> 4];
    r820t_write_reg(priv, 0x0A, a);
    r820t_write_reg(priv, 0x0B, b);
}