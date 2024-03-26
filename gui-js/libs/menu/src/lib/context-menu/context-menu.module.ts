import { CommonModule } from '@angular/common';
import { NgModule } from '@angular/core';
import { ColorPickerModule } from 'ngx-color-picker';
import { ContextMenuRoutingModule } from './context-menu-routing.module';
import { PenStylesComponent } from '@minsky/ui-components';
import { FormsModule, ReactiveFormsModule } from '@angular/forms';

@NgModule({
    imports: [
        CommonModule,
        ContextMenuRoutingModule,
        ColorPickerModule,
        FormsModule,
        ReactiveFormsModule,
        PenStylesComponent
    ],
})
export class ContextMenuModule {}
